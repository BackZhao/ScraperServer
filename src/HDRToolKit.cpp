#include "HDRToolKit.h"

#include <map>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dovi_meta.h>
#include <libavutil/log.h>
}

#include "Logger.h"

const std::size_t DOLBY_CONF_SIDE_DATA_SIZE = 8; // 杜比配置文件的数据大小

// 参考代码仓库: https://github.com/jellyfin/jellyfin.git
// 参考代码文件: MediaBrowser.Model/Entities/MediaStream.cs
// 参考代码函数: public (VideoRange VideoRange, VideoRangeType VideoRangeType) GetVideoColorRange()
VideoRangeType HDRToolKit::GetHDRTypeByDolbyConf(const uint8_t *data, std::size_t dataSize)
{
    if (dataSize != DOLBY_CONF_SIDE_DATA_SIZE) {
        return VideoRangeType::UNKNOWN;
    }

    const AVDOVIDecoderConfigurationRecord *dovi = reinterpret_cast<const AVDOVIDecoderConfigurationRecord *>(data);
    switch (dovi->dv_profile) {
        case 5: {
            return VideoRangeType::DOVI;
        }
        case 7: {
            return VideoRangeType::HDR10;
        }
        case 8: {
            static std::map<uint8_t, VideoRangeType> dvCompatID2HDRType = {
                {1, VideoRangeType::DOVI_WITH_HDR10},
                {2, VideoRangeType::DOVI_WITH_SDR},
                {4, VideoRangeType::DOVI_WITH_HLG},
                {6, VideoRangeType::DOVI_WITH_HDR10},
            };
            auto iter = dvCompatID2HDRType.find(dovi->dv_bl_signal_compatibility_id);
            if (iter != dvCompatID2HDRType.end()) {
                return iter->second;
            } else {
                return VideoRangeType::SDR;
            }
        };
        default: {
            return VideoRangeType::SDR;
        }
    }
}

VideoRangeType HDRToolKit::GetHDRTypeFromFile(const std::string &fileName)
{
    VideoRangeType hdrType = VideoRangeType::UNKNOWN;

    // 屏蔽ffmpeg告警
    av_log_set_level(AV_LOG_ERROR);
    avformat_network_init();

    AVFormatContext *format_ctx = NULL;
    if (avformat_open_input(&format_ctx, fileName.c_str(), NULL, NULL) < 0) {
        LOG_ERROR("Open file {} failed!", fileName);
        return VideoRangeType::UNKNOWN;
    }
    
    // 设置 analyzeduration 和 probesize
    format_ctx->max_analyze_duration = 200000000LL; // 设置 analyzeduration 值，单位：微秒
    format_ctx->probesize = 1000000000LL; // 设置 probesize 值，单位：字节

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        LOG_ERROR("No stream info found in file {}", fileName);
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    // 找到第一条视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOG_ERROR("No video stream found in file {}", fileName);
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    AVStream          *video_stream = format_ctx->streams[video_stream_index];
    AVCodecParameters *codecpar     = video_stream->codecpar;

    // 检测是否为视频codec
    if (codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
        LOG_ERROR("The selected stream is not a video stream, file: {}, codec type: ", fileName, codecpar->codec_type);
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    // 查找视频流解码器
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        LOG_ERROR("Unsupported codec for the video stream, file: {}", fileName);
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    // 分配codec上下文
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        LOG_ERROR("Could not allocate codec context");
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    // 从输入流拷贝codec参数到输出codec上下文
    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
        LOG_ERROR("Could not copy codec parameters to codec context");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    // 初始化给定AVCodec的上下文
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        LOG_ERROR("Could not copy codec parameters to codec context");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return VideoRangeType::UNKNOWN;
    }

    // 通过AVDOVIDecoderConfigurationRecord来获取视频的HDR类型
    if (codec_ctx->coded_side_data) {
        if (codec_ctx->coded_side_data->type == AV_PKT_DATA_DOVI_CONF) {
            hdrType = GetHDRTypeByDolbyConf(codec_ctx->coded_side_data->data, codec_ctx->coded_side_data->size);
        }
    } else {
        if (codecpar->color_trc == AVCOL_TRC_SMPTE2084) {
            hdrType = VideoRangeType::HDR10;
        } else if (codecpar->color_trc == AVCOL_TRC_ARIB_STD_B67) {
            hdrType = VideoRangeType::HLG;
        } else {
            hdrType = VideoRangeType::SDR;
        }
    }

    // 清除收尾
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    avformat_network_deinit();

    return hdrType;
}

#include "HDRToolKit.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <sstream>

#include <Poco/Exception.h>
#include <Poco/File.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>
#include <Poco/StreamCopier.h>

#include "Config.h"
#include "Logger.h"

const Poco::Process::Args FFPROBE_CHECK_ARGS            = {"-version"};      // 检测ffprobe的命令行参数
const std::string         FFRPOBE_CHECK_OUTPUT_KEYWORDS = "ffprobe version"; // 检测ffprobe的输出关键字
const Poco::Process::Args FFPROBE_ANALYZE_ARGS          = {"-analyzeduration",
                                                           "200M",
                                                           "-probesize",
                                                           "1G",
                                                           "-threads",
                                                           "0",
                                                           "-v",
                                                           "error",
                                                           "-print_format",
                                                           "json",
                                                           "-show_streams",
                                                           "-show_format",
                                                           "-i",
                                                           "file:"}; // 检测ffprobe的输出关键字

bool HDRToolKit::Checkffprobe()
{
    const std::string &ffprobePath = Config::Instance().GetffprobePath();

    if (ffprobePath.empty()) {
        LOG_ERROR("ffprobe path is empty!");
        return false;
    }

    if (!Poco::File(ffprobePath).exists()) {
        LOG_ERROR("Given ffprobe path doesn't exist: {}", ffprobePath);
        return false;
    }

    // 通过检查版本号命令参数的输出, 判断是否为ffprobe
    LOG_INFO("Checking ffprobe {}...", ffprobePath);
    Poco::Pipe            outPipe;
    Poco::ProcessHandle   ph = Poco::Process::launch(ffprobePath, FFPROBE_CHECK_ARGS, nullptr, &outPipe, nullptr);
    Poco::PipeInputStream inputStream(outPipe);
    std::stringstream     outStream;
    Poco::StreamCopier::copyStream(inputStream, outStream);
    ph.wait();
    if (outStream.str().find(FFRPOBE_CHECK_OUTPUT_KEYWORDS) != 0) {
        LOG_ERROR("ffprobe check output not match keyword: {}, actual output: \n{}",
                  FFRPOBE_CHECK_OUTPUT_KEYWORDS,
                  outStream.str());
        return false;
    }

    LOG_INFO("ffprobe {} works fine!", ffprobePath);
    return true;
}

// 参考代码仓库: https://github.com/jellyfin/jellyfin.git
// 参考代码文件: MediaBrowser.Model/Entities/MediaStream.cs
// 参考代码函数: public (VideoRange VideoRange, VideoRangeType VideoRangeType) GetVideoColorRange()
VideoRangeType HDRToolKit::GetHDRTypeByDolbyConf(int dv_profile, int dv_bl_signal_compatibility_id)
{
    switch (dv_profile) {
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
            auto iter = dvCompatID2HDRType.find(dv_bl_signal_compatibility_id);
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
    const std::string &ffprobePath = Config::Instance().GetffprobePath();
    LOG_DEBUG("Get hdr type for video {}...", fileName);

    Poco::Pipe                 outPipe;
    static Poco::Process::Args args      = FFPROBE_ANALYZE_ARGS;
    args.back()                          = "file:" + fileName;
    Poco::ProcessHandle   ffprobeProcHdl = Poco::Process::launch(ffprobePath, args, nullptr, &outPipe, nullptr);
    Poco::PipeInputStream inputStream(outPipe);

    Poco::JSON::Object::Ptr ffprobeJsonPtr;
    try {
        Poco::JSON::Parser jsonParser;
        ffprobeJsonPtr = jsonParser.parse(inputStream).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &e) {
        LOG_ERROR("ffprobe output parsed failed as json: {}", e.displayText());
        std::stringstream outStream;
        Poco::StreamCopier::copyStream(inputStream, outStream);
        LOG_ERROR("ffprobe output:\n{}", outStream.str());
        ffprobeProcHdl.wait();
    }

    ffprobeProcHdl.wait();

    Poco::JSON::Array::Ptr streams = ffprobeJsonPtr->getArray("streams");
    if (streams.isNull()) {
        LOG_WARN("No streams found in video {}", fileName);
        return VideoRangeType::UNKNOWN;
    }

    for (unsigned int streamInd = 0; streamInd < streams->size(); streamInd++) {
        Poco::JSON::Object::Ptr stream = streams->getObject(streamInd);
        if (stream->optValue<std::string>("codec_type", "") == "video") {
            Poco::JSON::Array::Ptr sideDataLists = stream->getArray("side_data_list");
            if (!sideDataLists.isNull()) {
                for (unsigned int sideDataListInd = 0; sideDataListInd < sideDataLists->size(); sideDataListInd++) {
                    Poco::JSON::Object::Ptr sideDataList = sideDataLists->getObject(sideDataListInd);
                    if (sideDataList->optValue<std::string>("side_data_type", "") == "DOVI configuration record") {
                        return GetHDRTypeByDolbyConf(sideDataList->optValue("dv_profile", -1),
                                                     sideDataList->optValue("dv_bl_signal_compatibility_id", -1));
                    }
                }
            }
            // 当不存在Dolby配置时, 通过视频的色彩传输标准来判断
            std::string colorTransfer = stream->optValue<std::string>("color_transfer", "");
            if (colorTransfer == "smpte2084") {
                return VideoRangeType::HDR10;
            } else if (colorTransfer == "arib-std-b67") {
                return VideoRangeType::HLG;
            } else {
                return VideoRangeType::SDR;
            }
        }
    }

    return VideoRangeType::UNKNOWN;
}

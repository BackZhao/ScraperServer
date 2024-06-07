#include "DataSource.h"

#include <Poco/SortedDirectoryIterator.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <locale>
#include <set>
#include <string>

// #include <MediaInfoDLL/MediaInfoDLL.h>
#include <Poco/AutoPtr.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/SortedDirectoryIterator.h>

#include "DataConvert.h"
#include "Logger.h"

using namespace Poco::XML;
using Poco::AutoPtr;

bool DataSource::m_isCancel = false;

bool DataSource::IsVideo(const std::string& suffix)
{
    // 比较文件名的最后四个字符(即后缀名)是否属于规定之内
    for (auto it = VIDEO_SUFFIX.begin(); it != VIDEO_SUFFIX.end(); it++) {
        if (*it == suffix) {
            return true;
        }
    }
    return false;
}

bool DataSource::IsNfoFormatMatch(const std::string& nfoPath)
{
    static const std::set<std::string> nodeNames = {
        "tvshow",
        "movie",
        "episodedetails",
    };

    // 获取根节点
    try {
        DOMParser         parser;
        AutoPtr<Document> dom         = parser.parse(nfoPath);
        auto              rootElement = dom->documentElement();
        // 根节点名称必须在指定范围内
        if (nodeNames.find(rootElement->nodeName()) == nodeNames.end()) {
            LOG_DEBUG("Nfo root node's name({}) doesn't match, path: {}", rootElement->nodeName(), nfoPath);
            return false;
        } else {
            return true;
        }
    } catch (Poco::Exception& e) {
        LOG_ERROR("Nfo parsed failed, reason: {}, path: {}", e.displayText(), nfoPath);
        return false;
    }
}

bool DataSource::IsJpgCompleted(const std::string& posterName)
{
    // JPEG头尾的标记码
    static std::vector<uint8_t> SOI = {0xFF, 0xD8};
    static std::vector<uint8_t> EOI = {0xFF, 0xD9};

    // 比较文件的头两字节和尾两字节与标记码
    std::ifstream ifs(posterName, std::ios::binary);
    if (ifs.is_open()) {
        static std::vector<uint8_t> firstTwoBytes(2);
        static std::vector<uint8_t> lastTwoBytes(2);
        ifs.read(reinterpret_cast<char*>(&firstTwoBytes[0]), firstTwoBytes.size());
        ifs.seekg(-(lastTwoBytes.size()), std::ios_base::end);
        ifs.read(reinterpret_cast<char*>(&lastTwoBytes[0]), lastTwoBytes.size());
        ifs.close();

        if (std::memcmp(firstTwoBytes.data(), SOI.data(), SOI.size()) != 0 ||
            std::memcmp(lastTwoBytes.data(), EOI.data(), EOI.size()) != 0) {
            LOG_ERROR("{} SOI({:#02X} {:#02X})/EOI({:#02X} {:#02X}) unmatched! JPEG signature: SOI(0xFF 0xD8)/EOI(0xFF 0xD9)!",
                      posterName,
                      firstTwoBytes[0],
                      firstTwoBytes[1],
                      lastTwoBytes[0],
                      lastTwoBytes[1]);
            return false;
        }
    } else {
        LOG_ERROR("{} open failed for checking: {}", posterName, strerror(errno));
        return false;
    }

    return true;
}

bool DataSource::IsPNGCompleted(const std::string& posterName)
{
    // PNG头尾的标识
    static std::vector<uint8_t> PNGsignature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    static std::vector<uint8_t> PNGIEND      = {0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

    // 比较文件的头两字节和尾两字节与标记码
    std::ifstream ifs(posterName, std::ios::binary);
    if (ifs.is_open()) {
        static std::vector<uint8_t> firstEightBytes(8);
        static std::vector<uint8_t> lastEightBytes(8);
        ifs.read(reinterpret_cast<char*>(&firstEightBytes[0]), firstEightBytes.size());
        ifs.seekg(-(lastEightBytes.size()), std::ios_base::end);
        ifs.read(reinterpret_cast<char*>(&lastEightBytes[0]), lastEightBytes.size());
        ifs.close();

        if (std::memcmp(firstEightBytes.data(), PNGsignature.data(), PNGsignature.size()) != 0 ||
            std::memcmp(lastEightBytes.data(), PNGIEND.data(), PNGIEND.size()) != 0) {
            LOG_ERROR("{} PNG imcomplete!", posterName);
            return false;
        }
    } else {
        LOG_ERROR("{} open failed for checking: {}", posterName, strerror(errno));
        return false;
    }

    return true;
}

void GetHdrFormat(VideoInfo& videoInfo)
{
    // using namespace MediaInfoDLL;

    // MediaInfo mi;
    // std::setlocale(LC_ALL, "en_US.utf8");

    // switch (videoInfo.videoType) {
    //     case MOVIE: {
    //         if (mi.Open(videoInfo.videoPath) <= 0) {
    //             LOG_ERROR("Failed to open with mediainfolib! path: {}", videoInfo.videoPath);
    //             videoInfo.hdrType = NON_HDR;
    //             return;
    //         }
    //         break;
    //     }
    //     case TV: {
    //         if (videoInfo.videoDetail.episodePaths.size() > 0) {
    //             // 以第一集的HDR类型填写
    //             if (mi.Open(videoInfo.videoDetail.episodePaths.at(0)) <= 0) {
    //                 LOG_ERROR("Failed to open with mediainfolib! path: {}", videoInfo.videoPath);
    //                 videoInfo.hdrType = NON_HDR;
    //                 return;
    //             }
    //         } else {
    //             LOG_ERROR("No eposide found! path: {}", videoInfo.videoPath);
    //             videoInfo.hdrType = NON_HDR;
    //         }
    //         break;
    //     }
    //     default:
    //         break;
    // }

    // if (mi.Count_Get(Stream_Video) < 1) {
    //     LOG_ERROR("Video stream not found with MediaInfoLib, set as NON-HDR! path: {}", videoInfo.videoPath);
    //     return;
    // }

    // std::string hdrFormat = mi.Get(Stream_Video, 0, "HDR_Format_Commercial");

    // // mediainfo打印的HDR_Format_Commercial可能结果
    // static const std::string DV_FORMAT           = "Dolby Vision";   // DV
    // static const std::string HDR10_FORMAT        = "HDR10";          // HDR10
    // static const std::string HDR10Plus_FORMAT    = "HDR10+";         // HDR10
    // static const std::string DV_AND_HDR10_FORMAT = "HDR10 / HDR10+"; // DV + HDR10

    // if (hdrFormat == DV_AND_HDR10_FORMAT) {
    //     videoInfo.hdrType = DOLBY_VISION_AND_HDR10;
    // } else if (hdrFormat == DV_FORMAT) {
    //     videoInfo.hdrType = DOLBY_VISION;
    // } else if (hdrFormat == HDR10_FORMAT) {
    //     videoInfo.hdrType = HDR10;
    // } else if (hdrFormat == HDR10Plus_FORMAT) {
    //     videoInfo.hdrType = HDR10Plus;
    // } else {
    //     videoInfo.hdrType = NON_HDR;
    // }

    // LOG_DEBUG("HDR type(parsed): {}, HDR format(MediaInfoLib): {}, path: {}", videoInfo.hdrType, hdrFormat, videoInfo.videoPath);
    videoInfo.hdrType = NON_HDR;
}

// TODO: 检查是否有多个匹配的海报和nfo文件(依据Kodi的wiki说明)
void DataSource::CheckVideoStatus(VideoInfo& videoInfo, bool forceDetectHdr)
{
    // 检查NFO文件是否存在
    auto CheckNfo = [&](const std::string& nfoName) {
        if (Poco::File(nfoName).exists()) {
            videoInfo.nfoStatus = IsNfoFormatMatch(nfoName) == true ? FILE_FORMAT_MATCH : FILE_FORMAT_MISMATCH;
            videoInfo.nfoPath   = nfoName;
            ParseNfoToVideoInfo(videoInfo);
        } else {
            videoInfo.nfoStatus = FILE_NOT_FOUND;
        }
    };

    auto CheckPoster = [&](const std::string& posterName) {
        if (Poco::File(posterName).exists()) {
            videoInfo.posterStatus = IsJpgCompleted(posterName) == true ? FILE_FORMAT_MATCH : FILE_FORMAT_MISMATCH;
            videoInfo.posterPath   = posterName;
        } else {
            videoInfo.posterStatus = FILE_NOT_FOUND;
        }
    };

    auto CheckFanart = [&](const std::string& fanartPath) {
        if (Poco::File(fanartPath).exists()) {
            videoInfo.fanartStatus = IsJpgCompleted(fanartPath) == true ? FILE_FORMAT_MATCH : FILE_FORMAT_MISMATCH;
            videoInfo.fanartPath   = fanartPath;
        } else {
            videoInfo.fanartStatus = FILE_NOT_FOUND;
        }
    };

    auto CheckClearlogo = [&](const std::string& clearlogoPath) {
        if (Poco::File(clearlogoPath).exists()) {
            videoInfo.clearlogoStatus = IsPNGCompleted(clearlogoPath) == true ? FILE_FORMAT_MATCH : FILE_FORMAT_MISMATCH;
            videoInfo.clearlogoPath   = clearlogoPath;
        } else {
            videoInfo.clearlogoStatus = FILE_NOT_FOUND;
        }
    };

    auto CheckEpisodes = [&]() {
        const auto& episodePaths = videoInfo.videoDetail.episodePaths;
        for (const auto& episodePath : episodePaths) {
            const std::string& baseNameWithDir =
                Poco::Path(episodePath).parent().toString() + Poco::Path(episodePath).getBaseName();
            if (Poco::File(baseNameWithDir + ".nfo").exists() &&  IsNfoFormatMatch(baseNameWithDir + ".nfo")) {
                videoInfo.videoDetail.episodeNfoCount++;
            }
        }
    };

    switch (videoInfo.videoType) {
        case MOVIE: {
            const std::string& baseNameWithDir =
                Poco::Path(videoInfo.videoPath).parent().toString() + Poco::Path(videoInfo.videoPath).getBaseName();
            videoInfo.nfoPath = baseNameWithDir + ".nfo";
            videoInfo.posterPath = baseNameWithDir + "-poster.jpg";
            videoInfo.fanartPath = baseNameWithDir + "-fanart.jpg";
            videoInfo.clearlogoPath = baseNameWithDir + "-clearlogo.jpg";
            CheckNfo(videoInfo.nfoPath);
            CheckPoster(videoInfo.posterPath);
            CheckFanart(videoInfo.fanartPath);
            CheckClearlogo(videoInfo.clearlogoPath);
            if (forceDetectHdr || videoInfo.nfoStatus != FILE_FORMAT_MATCH) {
                GetHdrFormat(videoInfo);
            } else {
                videoInfo.hdrType = NON_HDR;
            }
            break;
        }

        // TODO: 支持TV的HDR检测
        case TV: {
            const std::string& dirName = videoInfo.videoPath + Poco::Path::separator();
            videoInfo.nfoPath          = dirName + "tvshow.nfo";
            videoInfo.posterPath       = dirName + "poster.jpg";
            videoInfo.fanartPath       = dirName + "fanart.jpg";
            videoInfo.clearlogoPath    = dirName + "clearlogo.jpg";
            CheckNfo(videoInfo.nfoPath);
            CheckPoster(videoInfo.posterPath);
            CheckFanart(videoInfo.fanartPath);
            CheckClearlogo(videoInfo.clearlogoPath);
            CheckEpisodes();
            if (forceDetectHdr || videoInfo.nfoStatus != FILE_FORMAT_MATCH) {
                GetHdrFormat(videoInfo);
            } else {
                videoInfo.hdrType = NON_HDR;
            }
            break;
        }

        case MOVIE_SET: {
            // TODO: 实现电影集的检查
            break;
        }

        default:
            break;
    }
}

bool DataSource::IsMetaCompleted(const VideoInfo& videoInfo)
{
    // NFO文件和海报任意一个不完整, 即视为不完整
    if (videoInfo.nfoStatus != FILE_FORMAT_MATCH || videoInfo.posterStatus != FILE_FORMAT_MATCH) {
        return false;
    }

    // 电视剧还需要剧集的NFO完整
    if (videoInfo.videoType == TV && videoInfo.videoDetail.episodeNfoCount != videoInfo.videoDetail.episodePaths.size()) {
        return false;
    }

    return true;
}

std::string DataSource::GetLargestFile(const std::string& path)
{
    // 遍历文件
    Poco::DirectoryIterator               iter(path);
    Poco::DirectoryIterator               end;
    std::string                                 largestVideoFile;
    std::vector<std::pair<std::string, size_t>> videos;
    while (iter != end) {
        if (IsVideo(iter.path().getExtension())) {
            // TODO: 检测软链是否存在
            videos.push_back(std::make_pair(iter->path(), iter->getSize()));
        }
        ++iter;
    }

    // 没有视频文件则返回空字符串
    if (videos.empty()) {
        return "";
    }

    // 按照视频文件的体积排序, 返回最大体积的文件
    sort(videos.begin(),
         videos.end(),
         [&](const std::pair<std::string, size_t>& a, const std::pair<std::string, size_t>& b) {
             return a.second < b.second;
         });
    return videos.back().first;
}

bool DataSource::ScanMovie(const std::vector<std::string>& paths,
                          std::vector<VideoInfo>&         videoInfos,
                          std::atomic<std::size_t>&       processedVideoNum,
                          bool                            forceDetectHdr)
{
    // 清除历史数据
    processedVideoNum = 0;

    // 遍历所有电影数据源
    for (const auto& moviePath : paths) {
        Poco::DirectoryIterator iter(moviePath); // TODO: 没有处理路径不存在的问题
        Poco::DirectoryIterator end;
        while (iter != end && !m_isCancel) {
            LOG_TRACE("Scanning file/directory {} ...", iter->path());
            // 电影的最大扫描深度为1
            if (iter->isFile()) { // 视频文件直接添加
                if (IsVideo(iter.path().getExtension())) {
                    LOG_TRACE("Found movie: {}", iter->path());
                    VideoInfo videoInfo(MOVIE, iter->path());
                    videoInfo.videoFiletype = NO_FOLDER;
                    videoInfos.push_back(videoInfo);
                }
            } else if (iter->isDirectory()) { // 仅添加目录下的最大视频文件
                const std::string& largestVideoFile = GetLargestFile(iter->path());
                if (!largestVideoFile.empty()) {
                    LOG_TRACE("Found movie: {}", largestVideoFile);
                    VideoInfo videoInfo(MOVIE, largestVideoFile);
                    videoInfo.videoFiletype = IN_FOLDER;
                    videoInfos.push_back(videoInfo);
                }
            }
            ++iter;
        }
    }

    for (auto& videoInfo : videoInfos) {
        CheckVideoStatus(videoInfo, forceDetectHdr);
        processedVideoNum++;
    }

    LOG_DEBUG("Scan movie finished.");
    return true;
}

bool DataSource::ScanTv(const std::vector<std::string>& paths,
                       std::vector<VideoInfo>&         videoInfos,
                       std::atomic<std::size_t>&       processedVideoNum,
                       bool                            forceDetectHdr)
{
    // 清除历史数据
    processedVideoNum = 0;

    // 获取电视剧剧集的路径, 即添加给定目录下所有的视频文件
    auto GetEpisodePaths = [&](VideoInfo& videoInfo) {
        Poco::SortedDirectoryIterator iter(videoInfo.videoPath);
        Poco::SortedDirectoryIterator end;

        while (iter != end) {
            LOG_TRACE("Scanning for episodes: {}", iter->path());
            if (IsVideo(iter.path().getExtension())) {
                videoInfo.videoDetail.episodePaths.push_back(iter->path());
            }
            ++iter;
        }
    };

    // 遍历所有电视剧数据源
    const auto& tvPaths = paths;
    for (const auto& tvPath : tvPaths) {
        Poco::DirectoryIterator iter(tvPath);
        Poco::DirectoryIterator end;
        while (iter != end && !m_isCancel) {
            // 电视剧仅添加一级目录
            if (iter->isDirectory()) {
                VideoInfo videoInfo(TV, iter->path());
                videoInfo.videoFiletype = IN_FOLDER;
                videoInfos.push_back(videoInfo);
            }
            ++iter;
        }
    }

    for (auto& videoInfo : videoInfos) {
        GetEpisodePaths(videoInfo);
        CheckVideoStatus(videoInfo, forceDetectHdr);
        processedVideoNum++;
    }

    LOG_DEBUG("Scan tv finished.");
    return true;
}

bool DataSource::Scan(VideoType                       videoType,
                      const std::vector<std::string>& paths,
                      std::vector<VideoInfo>&         videoInfos,
                      std::atomic<std::size_t>&       processedVideoNum,
                      bool                            forceDetectHdr)
{
    LOG_DEBUG("Scanning for type {}...", VIDEO_TYPE_TO_STR.at(videoType));

    videoInfos.clear();

    /* clang-format off */
    // 扫描视频的函数映射表
    using namespace std::placeholders;
    static std::map<VideoType, std::function<bool(const std::vector<std::string>&, std::vector<VideoInfo>&, std::atomic<std::size_t>&, bool)>> scanFunc = {
        {MOVIE,     std::bind(&DataSource::ScanMovie,    _1, _2, _3, _4)},
        {TV,        std::bind(&DataSource::ScanTv,       _1, _2, _3, _4)},
        {MOVIE_SET, std::bind(&DataSource::ScanMovieSet, _1, _2, _3, _4)},
    };
    /* clang-format on */

    // TODO: paths索引检测
    return scanFunc.at(videoType)(paths, videoInfos, processedVideoNum, forceDetectHdr);
}

void DataSource::Cancel()
{
    LOG_INFO("Cancel all the scanning jobs...");
    m_isCancel = true;
}
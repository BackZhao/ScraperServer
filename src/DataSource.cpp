#include "DataSource.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <locale>
#include <set>
#include <string>

#include <MediaInfoDLL/MediaInfoDLL.h>
#include <Poco/AutoPtr.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
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
    static uint16_t SOI = 0xD8FF;
    static uint16_t EOI = 0xD9FF;

    // 比较文件的头两字节和尾两字节与标记码
    std::ifstream ifs(posterName, std::ios::binary);
    if (ifs.is_open()) {
        uint16_t firstTwoBytes = 0;
        uint16_t lastTwoBytes  = 0;
        ifs.read(reinterpret_cast<char*>(&firstTwoBytes), sizeof(firstTwoBytes));
        ifs.seekg(-(sizeof(lastTwoBytes)), std::ios_base::end);
        ifs.read(reinterpret_cast<char*>(&lastTwoBytes), sizeof(lastTwoBytes));
        ifs.close();

        if (firstTwoBytes != SOI || lastTwoBytes != EOI) {
            LOG_ERROR("{} SOI({:0X})/EOI({:0X}) unmatched(JPEG SOI 0XD8FF, EOI 0xD9FF)!",
                      posterName,
                      firstTwoBytes,
                      lastTwoBytes);
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
    using namespace MediaInfoDLL;

    MediaInfo mi;
    std::setlocale(LC_ALL, "en_US.utf8");

    switch (videoInfo.videoType) {
        case MOVIE: {
            if (mi.Open(videoInfo.videoPath) <= 0) {
                LOG_ERROR("Failed to open with mediainfolib! path: {}", videoInfo.videoPath);
                videoInfo.hdrType = NON_HDR;
                return;
            }
            break;
        }
        case TV: {
            if (videoInfo.videoDetail.episodePaths.size() > 0) {
                // 以第一集的HDR类型填写
                if (mi.Open(videoInfo.videoDetail.episodePaths.at(0)) <= 0) {
                    LOG_ERROR("Failed to open with mediainfolib! path: {}", videoInfo.videoPath);
                    videoInfo.hdrType = NON_HDR;
                    return;
                }
            } else {
                LOG_ERROR("No eposide found! path: {}", videoInfo.videoPath);
                videoInfo.hdrType = NON_HDR;
            }
            break;
        }
        default:
            break;
    }

    if (mi.Count_Get(Stream_Video) < 1) {
        LOG_ERROR("Video stream not found with MediaInfoLib, set as NON-HDR! path: {}", videoInfo.videoPath);
        return;
    }

    std::string hdrFormat = mi.Get(Stream_Video, 0, "HDR_Format_Commercial");

    // mediainfo打印的HDR_Format_Commercial可能结果
    static const std::string DV_FORMAT           = "Dolby Vision";   // DV
    static const std::string HDR10_FORMAT        = "HDR10";          // HDR10
    static const std::string HDR10Plus_FORMAT    = "HDR10+";         // HDR10
    static const std::string DV_AND_HDR10_FORMAT = "HDR10 / HDR10+"; // DV + HDR10

    if (hdrFormat == DV_AND_HDR10_FORMAT) {
        videoInfo.hdrType = DOLBY_VISION_AND_HDR10;
    } else if (hdrFormat == DV_FORMAT) {
        videoInfo.hdrType = DOLBY_VISION;
    } else if (hdrFormat == HDR10_FORMAT) {
        videoInfo.hdrType = HDR10;
    } else if (hdrFormat == HDR10Plus_FORMAT) {
        videoInfo.hdrType = HDR10Plus;
    } else {
        videoInfo.hdrType = NON_HDR;
    }

    LOG_DEBUG("HDR type(parsed): {}, HDR format(MediaInfoLib): {}, path: {}", videoInfo.hdrType, hdrFormat, videoInfo.videoPath);
}

// TODO: 检查是否有多个匹配的海报和nfo文件(依据Kodi的wiki说明)
void DataSource::CheckVideoStatus(VideoInfo& videoInfo, bool forceDetectHdr)
{
    // 检查NFO文件是否存在
    auto CheckNfo = [&](const std::string& nfoName) {
        if (Poco::File(nfoName).exists()) {
            videoInfo.nfoStatus = IsNfoFormatMatch(nfoName) == true ? NFO_FORMAT_MATCH : NFO_FORMAT_MISMATCH;
            videoInfo.nfoPath   = nfoName;
            ParseNfoToVideoInfo(videoInfo);
        } else {
            videoInfo.nfoStatus = NFO_NOT_FOUND;
        }
    };

    auto CheckPoster = [&](const std::string& posterName) {
        if (Poco::File(posterName).exists()) {
            videoInfo.posterStatus = IsJpgCompleted(posterName) == true ? POSTER_COMPELETED : POSTER_INCOMPELETED;
            videoInfo.posterPath   = posterName;
        } else {
            videoInfo.posterStatus = POSTER_NOT_FOUND;
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
            CheckNfo(videoInfo.nfoPath);
            CheckPoster(videoInfo.posterPath);
            if (forceDetectHdr || videoInfo.nfoStatus != NFO_FORMAT_MATCH) {
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
            CheckNfo(videoInfo.nfoPath);
            CheckPoster(videoInfo.posterPath);
            CheckEpisodes();
            if (forceDetectHdr || videoInfo.nfoStatus != NFO_FORMAT_MATCH) {
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
    if (videoInfo.nfoStatus != NFO_FORMAT_MATCH || videoInfo.posterStatus != POSTER_COMPELETED) {
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
    Poco::SortedDirectoryIterator               iter(path);
    Poco::SortedDirectoryIterator               end;
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

bool DataSource::ScanMovie(const std::vector<std::string>& paths, std::vector<VideoInfo>& videoInfos, bool forceDetectHdr)
{
    // 遍历所有电影数据源
    for (const auto& moviePath : paths) {
        Poco::SortedDirectoryIterator iter(moviePath);
        Poco::SortedDirectoryIterator end;
        while (iter != end && !m_isCancel) {
            LOG_TRACE("Scanning file/directory {} ...", iter->path());
            // 电影的最大扫描深度为1
            if (iter->isFile()) { // 视频文件直接添加
                if (IsVideo(iter.path().getExtension())) {
                    LOG_TRACE("Found movie: {}", iter->path());
                    VideoInfo videoInfo(MOVIE, iter->path());
                    CheckVideoStatus(videoInfo, forceDetectHdr);
                    videoInfos.push_back(videoInfo);
                }
            } else if (iter->isDirectory()) { // 仅添加目录下的最大视频文件
                const std::string& largestVideoFile = GetLargestFile(iter->path());
                if (!largestVideoFile.empty()) {
                    LOG_TRACE("Found movie: {}", largestVideoFile);
                    VideoInfo videoInfo(MOVIE, largestVideoFile);
                    CheckVideoStatus(videoInfo, forceDetectHdr);
                    videoInfos.push_back(videoInfo);
                }
            }
            ++iter;
        }
    }

    LOG_DEBUG("Scan movie finished.");
    return true;
}

bool DataSource::ScanTv(const std::vector<std::string>& paths, std::vector<VideoInfo>& videoInfos, bool forceDetectHdr)
{
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
        Poco::SortedDirectoryIterator iter(tvPath);
        Poco::SortedDirectoryIterator end;
        while (iter != end && !m_isCancel) {
            // 电视剧仅添加一级目录
            if (iter->isDirectory()) {
                VideoInfo videoInfo(TV, iter->path());
                GetEpisodePaths(videoInfo);
                CheckVideoStatus(videoInfo, forceDetectHdr);
                videoInfos.push_back(videoInfo);
            }
            ++iter;
        }
    }

    LOG_DEBUG("Scan tv finished.");
    return true;
}

bool DataSource::Scan(VideoType                                            videoType,
                      const std::map<VideoType, std::vector<std::string>>& paths,
                      std::map<VideoType, std::vector<VideoInfo>>&         videoInfos,
                      bool forceDetectHdr)
{
    LOG_DEBUG("Scanning for type {}...", VIDEO_TYPE_TO_STR.at(videoType));

    videoInfos.at(videoType).clear();

    /* clang-format off */
    // 扫描视频的函数映射表
    using namespace std::placeholders;
    static std::map<VideoType, std::function<bool(const std::vector<std::string>&, std::vector<VideoInfo>&, bool)>> scanFunc = {
        {MOVIE,     std::bind(&DataSource::ScanMovie,    _1, _2, _3)},
        {TV,        std::bind(&DataSource::ScanTv,       _1, _2, _3)},
        {MOVIE_SET, std::bind(&DataSource::ScanMovieSet, _1, _2, _3)},
    };
    /* clang-format on */

    // TODO: paths索引检测
    return scanFunc.at(videoType)(paths.at(videoType), videoInfos.at(videoType), forceDetectHdr);
}

void DataSource::Cancel()
{
    LOG_INFO("Cancel all the scanning jobs...");
    m_isCancel = true;
}
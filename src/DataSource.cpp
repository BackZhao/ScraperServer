#include "DataSource.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <regex>
#include <set>
#include <string>

#include <Poco/AutoPtr.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/SortedDirectoryIterator.h>
#include <Poco/String.h>

#include "CommonType.h"
#include "DataConvert.h"
#include "HDRToolKit.h"
#include "Logger.h"

using namespace Poco::XML;
using Poco::AutoPtr;

bool DataSource::m_isCancel     = false;
bool DataSource::m_ffprobeReady = false;

bool DataSource::IsVideo(const std::string& suffix)
{
    // 比较文件名的最后四个字符(即后缀名)是否属于规定之内
    for (auto it = VIDEO_SUFFIX.begin(); it != VIDEO_SUFFIX.end(); it++) {
        if (Poco::icompare(*it, suffix) == 0) {
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
            LOG_ERROR("{} SOI({:#02X} {:#02X})/EOI({:#02X} {:#02X}) unmatched! JPEG signature: SOI(0xFF 0xD8)/EOI(0xFF "
                      "0xD9)!",
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
    std::string videoPath;
    switch (videoInfo.videoType) {
        case VideoType::MOVIE: {
            videoPath = videoInfo.videoPath;
            break;
        }
        case VideoType::TV: {
            videoPath = videoInfo.videoDetail.episodePaths.front(); // 电视剧取第一集来检测HDR格式
            break;
        }
        default: {
            LOG_WARN("Unsupported video type to check HDR format!");
            return;
        }
    }

    videoInfo.hdrType = HDRToolKit::GetHDRTypeFromFile(videoPath);
    LOG_DEBUG("VideoRangeType: {}, {}", VIDEO_RANGE_TYPE_TO_STR_MAP.at(videoInfo.hdrType), videoInfo.videoPath);
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

    // TODO: 合并检测图片的lambda
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
            videoInfo.clearlogoStatus =
                IsPNGCompleted(clearlogoPath) == true ? FILE_FORMAT_MATCH : FILE_FORMAT_MISMATCH;
            videoInfo.clearlogoPath = clearlogoPath;
        } else {
            videoInfo.clearlogoStatus = FILE_NOT_FOUND;
        }
    };

    auto CheckEpisodes = [&]() {
        const auto& episodePaths = videoInfo.videoDetail.episodePaths;
        for (const auto& episodePath : episodePaths) {
            const std::string& baseNameWithDir =
                Poco::Path(episodePath).parent().toString() + Poco::Path(episodePath).getBaseName();
            if (Poco::File(baseNameWithDir + ".nfo").exists() && IsNfoFormatMatch(baseNameWithDir + ".nfo")) {
                videoInfo.videoDetail.episodeNfoCount++;
            }
        }
    };

    switch (videoInfo.videoType) {
        case MOVIE: {
            const std::string& baseNameWithDir =
                Poco::Path(videoInfo.videoPath).parent().toString() + Poco::Path(videoInfo.videoPath).getBaseName();
            videoInfo.nfoPath       = baseNameWithDir + ".nfo";
            videoInfo.posterPath    = baseNameWithDir + "-poster.jpg";
            videoInfo.fanartPath    = baseNameWithDir + "-fanart.jpg";
            videoInfo.clearlogoPath = baseNameWithDir + "-clearlogo.jpg";
            CheckNfo(videoInfo.nfoPath);
            CheckPoster(videoInfo.posterPath);
            CheckFanart(videoInfo.fanartPath);
            CheckClearlogo(videoInfo.clearlogoPath);
            if (m_ffprobeReady && forceDetectHdr) {
                GetHdrFormat(videoInfo);
            } else {
                videoInfo.hdrType = VideoRangeType::SDR;
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
            if (m_ffprobeReady && forceDetectHdr) {
                GetHdrFormat(videoInfo);
            } else {
                videoInfo.hdrType = VideoRangeType::SDR;
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
    if (videoInfo.videoType == TV &&
        videoInfo.videoDetail.episodeNfoCount != videoInfo.videoDetail.episodePaths.size()) {
        return false;
    }

    return true;
}

std::string DataSource::GetLargestFile(const std::string& path)
{
    // 遍历文件
    Poco::DirectoryIterator                     iter(path);
    Poco::DirectoryIterator                     end;
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

void DataSource::GetVideoPathesFromMovieSet(const std::string& path, std::vector<VideoInfo>& videoInfos)
{
    // TODO: 还是需要处理成Kodi的电影集, 否则无法刮削不含子目录的情况
    Poco::DirectoryIterator iter(path);
    Poco::DirectoryIterator end;

    std::vector<std::string> videoPathes;
    while (iter != end && !m_isCancel) {
        // 当前为目录, 目录下没有视频文件, 但是有多个子目录, 子目录内有视频文件, 则判定为电影集,
        // 收录每个子目录内的最大视频文件
        if (iter->isDirectory()) { // 收录视频文件
            const std::string& largestVideoFile = GetLargestFile(iter->path());
            if (!largestVideoFile.empty()) {
                LOG_TRACE("Found videos: {}", largestVideoFile);
                videoPathes.push_back(largestVideoFile);
            }
        }
        ++iter;
    }

    // 如果扫描到的视频个数不足两个, 则认为该目录并非电影集
    if (videoPathes.size() <= 1) {
        LOG_WARN("Only {} videos accepted, not treated as movie set: {}", videoPathes.size(), path);
    } else {
        // TODO: 适配Kodi电影集元数据, 目前均视为独立电影, 单独刮削
        for (auto videoPath : videoPathes) {
            LOG_TRACE("Found movie (in movie set): {}", videoPath);
            VideoInfo videoInfo(MOVIE, videoPath);
            videoInfo.videoFiletype = IN_FOLDER;
            videoInfos.push_back(videoInfo);
        }
    }
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
        if (!Poco::File(moviePath).exists()) {
            LOG_ERROR("Given path {} doesn't exist", moviePath);
            continue;
        }

        if (!Poco::File(moviePath).isDirectory()) {
            LOG_ERROR("Given path {} is not a directory", moviePath);
            continue;
        }

        Poco::DirectoryIterator iter(moviePath);
        Poco::DirectoryIterator end;
        while (iter != end && !m_isCancel) {
            LOG_TRACE("Scanning file/directory {} ...", iter->path());
            // 电影的扫描逻辑:
            // 1. 当前为视频文件, 直接收录
            // 2. 当前为目录, 目录下有视频文件, 收录最大的视频文件(为了排除samples等短片)
            // 3. 当前为目录, 目录下没有视频文件, 但是有多个子目录, 子目录内有视频文件, 则判定为电影集,
            // 收录每个子目录内的最大视频文件
            if (iter->isFile()) { // 视频文件直接添加
                if (IsVideo(iter.path().getExtension())) {
                    LOG_TRACE("Found movie: {}", iter->path());
                    VideoInfo videoInfo(MOVIE, iter->path());
                    videoInfo.videoFiletype = NO_FOLDER;
                    videoInfos.push_back(videoInfo);
                }
            } else if (iter->isDirectory()) { // 收录视频文件
                const std::string& largestVideoFile = GetLargestFile(iter->path());
                if (!largestVideoFile.empty()) {
                    LOG_TRACE("Found movie: {}", largestVideoFile);
                    VideoInfo videoInfo(MOVIE, largestVideoFile);
                    videoInfo.videoFiletype = IN_FOLDER;
                    videoInfos.push_back(videoInfo);
                } else {
                    GetVideoPathesFromMovieSet(iter->path(), videoInfos);
                }
            }
            ++iter;
        }
    }

    for (auto& videoInfo : videoInfos) {
        if (m_isCancel) {
            return false;
        }
        CheckVideoStatus(videoInfo, forceDetectHdr);
        processedVideoNum++;
    }

    LOG_DEBUG("Scan movie finished.");
    return true;
}

// generate by ChatGPT
std::pair<int, int> ExtractSeasonAndEpisode(const std::string& name)
{
    std::regex  pattern(R"([sS](\d+)[eE](\d+))");
    std::smatch match;

    if (std::regex_search(name, match, pattern)) {
        return {std::stoi(match[1]), std::stoi(match[2])};
    }
    // 无法解析的返回 (-1, -1)
    return {-1, -1};
}

bool CompareEpisodes(const std::string& a, const std::string& b)
{
    std::pair<int, int> seasonEpisodeA = ExtractSeasonAndEpisode(a);
    std::pair<int, int> seasonEpisodeB = ExtractSeasonAndEpisode(b);

    int seasonA  = seasonEpisodeA.first;
    int episodeA = seasonEpisodeA.second;
    int seasonB  = seasonEpisodeB.first;
    int episodeB = seasonEpisodeB.second;

    // 如果无法解析，直接返回 false，保持原有顺序
    if (seasonA == -1 && seasonB == -1) {
        return false;
    }
    if (seasonA == -1) {
        return false; // a 排在 b 后面
    }
    if (seasonB == -1) {
        return true; // b 排在 a 后面
    }

    // 先比较季
    if (seasonA != seasonB) {
        return seasonA < seasonB;
    }
    // 如果季相同，比较集
    // 如果集数相同, 则按照字母序排序
    if (episodeA == episodeB) {
        return a < b;
    }
    return episodeA < episodeB;
}

void SortEpisodes(std::vector<std::string>& episodePaths)
{
    if (episodePaths.empty()) {
        return;
    }

    std::pair<int, int> seasonEpisodeA = ExtractSeasonAndEpisode(episodePaths.front());
    if (seasonEpisodeA.first == -1 || seasonEpisodeA.second == -1) {
        // 如果剧集命名不是SxxExx格式的, 则按照字母序排序
        // TODO: 如果剧集包含的数字超过三位, 则仍然需要提取数字处理
        std::sort(episodePaths.begin(), episodePaths.end());
    } else {
        std::sort(episodePaths.begin(), episodePaths.end(), CompareEpisodes);
    }
}

void DataSource::GetVideoPathesFromTVSet(const std::string& path, std::vector<VideoInfo>& videoInfos)
{
    // TODO: 最好按照剧集合集来处理, 当前按照独立的剧集来处理的
    Poco::DirectoryIterator iter(path);
    Poco::DirectoryIterator end;

    std::vector<VideoInfo> tempVideoInfos;
    while (iter != end && !m_isCancel) {
        // 当前为目录, 目录下没有视频文件, 但是有多个子目录, 子目录内有视频文件, 则判定为电视剧合集
        if (iter->isDirectory()) {
            auto episodePaths = GetEpisodePaths(iter->path());
            if (!episodePaths.empty()) {
                VideoInfo videoInfo(TV, iter->path());
                videoInfo.videoDetail.episodePaths = episodePaths;
                tempVideoInfos.push_back(videoInfo);
            } else {
                LOG_WARN("No episodes found in {}", iter->path());
            }
        }
        ++iter;
    }

    // 如果扫描到的视频个数不足两个, 则认为该目录并非电影集
    if (tempVideoInfos.size() <= 1) {
        LOG_WARN("Only {} tv shows accepted, not treated as tv set: {}", tempVideoInfos.size(), path);
    } else {
        for (auto videoInfo : tempVideoInfos) {
            LOG_TRACE("Found tv show (in tv set): {}", videoInfo.videoPath);
            videoInfos.push_back(videoInfo);
        }
    }
}

std::vector<std::string> DataSource::GetEpisodePaths(const std::string& tvPath)
{
    Poco::DirectoryIterator iter(tvPath);
    Poco::DirectoryIterator end;

    std::vector<std::string> episodePaths;
    while (iter != end && !m_isCancel) {
        // LOG_TRACE("Scanning for episodes: {}", iter->path());
        if (IsVideo(iter.path().getExtension())) {
            episodePaths.push_back(iter->path());
        }
        ++iter;
    }
    SortEpisodes(episodePaths);
    return episodePaths;
}

bool DataSource::ScanTv(const std::vector<std::string>& paths,
                        std::vector<VideoInfo>&         videoInfos,
                        std::atomic<std::size_t>&       processedVideoNum,
                        bool                            forceDetectHdr)
{
    // 清除历史数据
    processedVideoNum = 0;

    // 遍历所有电视剧数据源
    const auto& tvPaths = paths;
    for (const auto& tvPath : tvPaths) {
        if (!Poco::File(tvPath).exists()) {
            LOG_ERROR("Given path {} doesn't exist", tvPath);
            continue;
        }

        if (!Poco::File(tvPath).isDirectory()) {
            LOG_ERROR("Given path {} is not a directory", tvPath);
            continue;
        }

        Poco::DirectoryIterator iter(tvPath);
        Poco::DirectoryIterator end;
        while (iter != end && !m_isCancel) {
            // 电视剧仅添加一级目录
            if (iter->isDirectory()) {
                // TODO: 处理电视剧合集
                auto episodePaths = GetEpisodePaths(iter->path());
                if (!episodePaths.empty()) {
                    VideoInfo videoInfo(TV, iter->path());
                    videoInfo.videoDetail.episodePaths = episodePaths;
                    videoInfo.videoFiletype            = IN_FOLDER;
                    videoInfos.push_back(videoInfo);
                } else {
                    GetVideoPathesFromTVSet(iter->path(), videoInfos);
                }
            }
            ++iter;
        }
    }

    for (auto& videoInfo : videoInfos) {
        if (m_isCancel) {
            return false;
        }
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

    m_ffprobeReady = HDRToolKit::Checkffprobe();

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
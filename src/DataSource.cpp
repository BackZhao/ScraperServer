#include "DataSource.h"

#include <algorithm>
#include <fstream>
#include <functional>

#include <Poco/SortedDirectoryIterator.h>

#include "DataConvert.h"
#include "Logger.h"

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
    // 本程序生成的NFO文件第一行固定为此格式
    static const std::string expectedFirstLine = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)";

    std::ifstream ifs(nfoPath);
    std::string   firstLine;
    std::getline(ifs, firstLine);

    // TODO: 是否需要解析XML是否完整?
    return firstLine == expectedFirstLine;
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
            return false;
        }
    } else {
        return false;
    }

    return true;
}

void DataSource::CheckVideoStatus(VideoInfo& videoInfo)
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
            if (IsNfoFormatMatch(baseNameWithDir + ".nfo")) {
                videoInfo.videoDetail.episodeNfoCount++;
            }
        }
    };

    switch (videoInfo.videoType) {
        case MOVIE: {
            const std::string& baseNameWithDir =
                Poco::Path(videoInfo.videoPath).parent().toString() + Poco::Path(videoInfo.videoPath).getBaseName();
            CheckNfo(baseNameWithDir + ".nfo");
            CheckPoster(baseNameWithDir + "-poster.jpg");
            break;
        }

        case TV: {
            const std::string& dirName = videoInfo.videoPath + Poco::Path::separator();
            CheckNfo(dirName + "tvshow.nfo");
            CheckPoster(dirName + "poster.jpg");
            CheckEpisodes();
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

std::string DataSource::GetLargestFile(const std::string& path)
{
    // 遍历文件
    Poco::SortedDirectoryIterator               iter(path);
    Poco::SortedDirectoryIterator               end;
    std::string                                 largestVideoFile;
    std::vector<std::pair<std::string, size_t>> videos;
    while (iter != end) {
        if (IsVideo(iter.path().getExtension())) {
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

bool DataSource::ScanMovie(const std::vector<std::string>& paths, std::vector<VideoInfo>& videoInfos)
{
    // 遍历所有电影数据源
    for (const auto& moviePath : paths) {
        Poco::SortedDirectoryIterator iter(moviePath);
        Poco::SortedDirectoryIterator end;
        while (iter != end) {
            // 电影的最大扫描深度为1
            if (iter->isFile()) { // 视频文件直接添加
                if (IsVideo(iter.path().getExtension())) {
                    VideoInfo videoInfo(MOVIE, iter->path());
                    CheckVideoStatus(videoInfo);
                    videoInfos.push_back(videoInfo);
                }
            } else if (iter->isDirectory()) { // 仅添加目录下的最大视频文件
                const std::string& largestVideoFile = GetLargestFile(iter->path());
                if (!largestVideoFile.empty()) {
                    VideoInfo videoInfo(MOVIE, largestVideoFile);
                    CheckVideoStatus(videoInfo);
                    videoInfos.push_back(videoInfo);
                }
            }
            ++iter;
        }
    }

    LOG_DEBUG("Scan movie finished.");
    return true;
}

bool DataSource::ScanTv(const std::vector<std::string>& paths, std::vector<VideoInfo>& videoInfos)
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
        while (iter != end) {
            // 电视剧仅添加一级目录
            if (iter->isDirectory()) {
                VideoInfo videoInfo(TV, iter->path());
                GetEpisodePaths(videoInfo);
                CheckVideoStatus(videoInfo);
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
                      std::map<VideoType, std::vector<VideoInfo>>&         videoInfos)
{
    LOG_DEBUG("Scanning for type {}...", VIDEO_TYPE_TO_STR.at(videoType));

    videoInfos.at(videoType).clear();

    /* clang-format off */
    // 扫描视频的函数映射表
    using namespace std::placeholders;
    static std::map<VideoType, std::function<bool(const std::vector<std::string>&, std::vector<VideoInfo>&)>> scanFunc = {
        {MOVIE,     std::bind(&DataSource::ScanMovie,    _1, _2)},
        {TV,        std::bind(&DataSource::ScanTv,       _1, _2)},
        {MOVIE_SET, std::bind(&DataSource::ScanMovieSet, _1, _2)},
    };
    /* clang-format on */

    return scanFunc.at(videoType)(paths.at(videoType), videoInfos.at(videoType));
}
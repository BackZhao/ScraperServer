#pragma once

#include "CommonType.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

class DataSource
{
public:

    static bool Scan(VideoType                                      videoType,
                     const std::map<VideoType, std::vector<std::string>>& paths,
                     std::map<VideoType, std::vector<VideoInfo>>&   videoInfos);

    static void Cancel();

    static void CheckVideoStatus(VideoInfo& videoInfo);

private:

    static bool IsVideo(const std::string& suffix);
    static bool IsNfoFormatMatch(const std::string& nfoPath);
    static bool IsJpgCompleted(const std::string& posterPath);

    static std::string GetLargestFile(const std::string& path);

    static bool ScanMovie(const std::vector<std::string>& path, std::vector<VideoInfo>& videoInfos);
    static bool ScanMovieSet(const std::vector<std::string>& path, std::vector<VideoInfo>& videoInfos)
    { /*TODO: 实现电影集的扫描*/
        return true;
    };
    static bool ScanTv(const std::vector<std::string>& path, std::vector<VideoInfo>& videoInfos);

private:

    static bool m_isCancel; // 是否取消扫描
};

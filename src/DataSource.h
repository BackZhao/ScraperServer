#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "CommonType.h"

class DataSource
{
public:

    static bool Scan(VideoType                       videoType,
                     const std::vector<std::string>& paths,
                     std::vector<VideoInfo>&         videoInfos,
                     std::atomic<std::size_t>&       processedVideoNum,
                     bool                            forceDetectHdr = false);

    static void Cancel();

    static void CheckVideoStatus(VideoInfo& videoInfo, bool forceDetectHdr);

    static bool IsMetaCompleted(const VideoInfo& videoInfo);

private:

    static bool IsVideo(const std::string& suffix);
    static bool IsNfoFormatMatch(const std::string& nfoPath);
    static bool IsJpgCompleted(const std::string& posterPath);
    static bool IsPNGCompleted(const std::string& posterPath);

    static void GetVideoPathesFromMovieSet(const std::string& path, std::vector<VideoInfo>& videoInfos);

    static std::string GetLargestFile(const std::string& path);

    static bool ScanMovie(const std::vector<std::string>& path,
                          std::vector<VideoInfo>&         videoInfos,
                          std::atomic<std::size_t>&       processedVideoNum,
                          bool                            forceDetectHdr);
    static bool ScanMovieSet(const std::vector<std::string>&,
                             std::vector<VideoInfo>&,
                             std::atomic<std::size_t>&,
                             bool)
    { /*TODO: 实现电影集的扫描*/
        return true;
    };
    static bool ScanTv(const std::vector<std::string>& path,
                       std::vector<VideoInfo>&         videoInfos,
                       std::atomic<std::size_t>&       processedVideoNum,
                       bool                            forceDetectHdr);

private:

    static bool m_isCancel; // 是否取消扫描
};

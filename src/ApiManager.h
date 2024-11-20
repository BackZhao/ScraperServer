#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>

#include <Poco/JSON/Object.h>
#include <Poco/LocalDateTime.h>

#include "DataSource.h"

class ApiManager
{
    enum ScanSatus {
        NEVER_SCANNED,
        SCANNING,
        SCANNING_FINISHED,
    };

    /**
     * @brief
     *
     */
    struct ScanInfo {
        ScanInfo() : scanStatus(NEVER_SCANNED)
        {
            // non-param constructor
        }

        ScanInfo(const ScanInfo &other)
            : scanStatus(other.scanStatus), scanBeginTime(other.scanBeginTime), scanEndTime(other.scanEndTime),
              clientAddr(other.clientAddr)
        {
            // copy constructor
        }

        ScanSatus                scanStatus;
        std::atomic<std::size_t> processedVideoNum;
        Poco::LocalDateTime      scanBeginTime;
        Poco::LocalDateTime      scanEndTime;
        std::string              clientAddr;
        std::mutex               lock;
    };

    enum RefreshStatus {
        NEVER_REFRESHED,
        REFRESHING,
        REFRESHING_FINISHED,
    };
    /**
     * @brief
     *
     */
    struct RefreshInfo {
        RefreshInfo() : refreshStatus(NEVER_REFRESHED)
        {
            // non-param constructor
        }

        RefreshInfo(const RefreshInfo &other)
            : refreshStatus(other.refreshStatus), refreshBeginTime(other.refreshBeginTime),
              refreshEndTime(other.refreshEndTime), clientAddr(other.clientAddr)
        {
            // copy constructor
        }

        RefreshStatus       refreshStatus;
        Poco::LocalDateTime refreshBeginTime;
        Poco::LocalDateTime refreshEndTime;
        std::string         clientAddr;
        std::mutex          lock;
    };

public:

    using ApiHandler = std::function<void(const Poco::JSON::Object &, std::ostream &)>;

    static ApiManager &Instance()
    {
        static ApiManager apiManager;
        return apiManager;
    }

    void SetScanPaths(std::map<VideoType, std::vector<std::string>> paths);

    void ScanAll();

    bool IsQuitting();

    void ProcessScan(VideoType videoType, bool forceDetectHdr);
    void Scan(const Poco::JSON::Object &param, std::ostream &out);
    void ScanResult(const Poco::JSON::Object&, std::ostream &out);
    void List(const Poco::JSON::Object &param, std::ostream &out);
    void Detail(const Poco::JSON::Object &param, std::ostream &out);
    void Scrape(const Poco::JSON::Object &param, std::ostream &out);
    void Refresh(const Poco::JSON::Object &param, std::ostream &out);
    void Quit(const Poco::JSON::Object &, std::ostream &out);
    void AutoUpdateTV();

    void ProcessRefresh(VideoType videoType);
    void RefreshResult(const Poco::JSON::Object &, std::ostream &out);
    void RefreshMovie();
    void RefreshTV();

    /**
     * @brief 获取内部日志
     *
     * @param param API请求参数
     * @param out API响应回填输出流
     */
    void InterLog(const Poco::JSON::Object &param, std::ostream &out);

    void Version(const Poco::JSON::Object &param, std::ostream &out);

private:

    ApiManager()
    {
        // 初始化map
        m_videoInfos = {
            {MOVIE, std::vector<VideoInfo>()},
            {MOVIE_SET, std::vector<VideoInfo>()},
            {TV, std::vector<VideoInfo>()},
        };

        m_scanInfos = {
            {MOVIE, ScanInfo()},
            {MOVIE_SET, ScanInfo()},
            {TV, ScanInfo()},
        };

        m_refreshInfos = {
            {MOVIE, RefreshInfo()},
            {MOVIE_SET, RefreshInfo()},
            {TV, RefreshInfo()},
        };
    };

private:

    std::map<VideoType, std::vector<std::string>> m_paths;
    std::map<VideoType, std::vector<VideoInfo>>   m_videoInfos;
    std::map<VideoType, ScanInfo>                 m_scanInfos;
    std::map<VideoType, RefreshInfo>              m_refreshInfos;

    std::atomic<bool> m_isQuitting{false}; // 是否正在退出
};
#pragma once

#include "DataSource.h"

#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include <Poco/JSON/Object.h>
#include <Poco/LocalDateTime.h>

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

        ScanSatus           scanStatus;
        Poco::LocalDateTime scanBeginTime;
        Poco::LocalDateTime scanEndTime;
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

    void ProcessScan(VideoType videoType);
    void Scan(const Poco::JSON::Object &param, std::ostream &out);
    void ScanResult(const Poco::JSON::Object &param, std::ostream &out);
    void List(const Poco::JSON::Object &param, std::ostream &out);
    void Detail(const Poco::JSON::Object &param, std::ostream &out);
    void AutoUpdateTV();

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
    };

private:

    std::map<VideoType, std::vector<std::string>> m_paths;
    std::map<VideoType, std::vector<VideoInfo>>   m_videoInfos;
    std::map<VideoType, ScanInfo>                 m_scanInfos;
};
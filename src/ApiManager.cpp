#include "ApiManager.h"

#include "DataConvert.h"
#include "Tmdb.h"
#include "Logger.h"

#include <fstream>

#include <Poco/DateTimeFormatter.h>

void ApiManager::SetScanPaths(std::map<VideoType, std::vector<std::string>> paths)
{
    m_paths = paths;
}

void ApiManager::ProcessScan(VideoType videoType)
{
    std::unique_lock<std::mutex> locker(m_scanInfos[videoType].lock, std::try_to_lock);
    m_scanInfos[videoType].scanStatus    = SCANNING;
    m_scanInfos[videoType].scanBeginTime = Poco::DateTime();
    DataSource::Scan(videoType, m_paths, m_videoInfos);
    m_scanInfos[videoType].scanEndTime = Poco::DateTime();
    m_scanInfos[videoType].scanStatus  = SCANNING_FINISHED;
}

void ApiManager::Scan(const Poco::JSON::Object &param, std::ostream &out)
{
    // 必须制定视频类型
    if (param.isNull("videoType")) {
        out << R"({"success": false, "msg": "Video type is not given!"})";
        return;
    }

    // 保证视频类型在可选范围内
    auto findResult = STR_TO_VIDEO_TYPE.find(param.getValue<std::string>("videoType"));
    if (findResult == STR_TO_VIDEO_TYPE.end()) {
        out << R"({"success": false, "msg": "Video type is invalid!"})";
        return;
    }
    VideoType videoType = findResult->second;

    // 无法加锁说明后台正在扫描
    std::unique_lock<std::mutex> locker(m_scanInfos[videoType].lock, std::try_to_lock);
    if (!locker.owns_lock()) {
        out << R"({"success": false, "msg": "Still scanning!"})";
        return;
    }

    // 加锁后在新线程进行扫描
    std::thread scanThread(std::bind(&ApiManager::ProcessScan, this, std::placeholders::_1), videoType);
    scanThread.detach();

    out << R"({"success": true, "msg": "Begin scanning!"})";
}

void ApiManager::ScanAll()
{
    for (int videoType = MOVIE; videoType < UNKNOWN_TYPE; videoType++) {
        if (m_paths.find(static_cast<VideoType>(videoType)) != m_paths.end()) {
            ProcessScan(static_cast<VideoType>(videoType));
        }
    }
}

void ApiManager::ScanResult(const Poco::JSON::Object&, std::ostream &out)
{
    Poco::JSON::Array outJsonArr;
    for (auto &scanInfoPair : m_scanInfos) {
        Poco::JSON::Object           scanInfoJsonObj;
        scanInfoJsonObj.set("VideoType", VIDEO_TYPE_TO_STR.at(scanInfoPair.first));
        auto& scanInfo = scanInfoPair.second;
        std::unique_lock<std::mutex> locker(scanInfo.lock, std::try_to_lock);
        if (!locker.owns_lock()) { // 避免原子性问题
            scanInfoJsonObj.set("ScanStatus", static_cast<int>(SCANNING));
            scanInfoJsonObj.set("ScanBeginTime",
                                Poco::DateTimeFormatter::format(scanInfo.scanBeginTime, "%Y-%m-%d %H:%M:%S"));
        } else if (scanInfo.scanStatus == NEVER_SCANNED) {
            scanInfoJsonObj.set("ScanStatus", static_cast<int>(scanInfo.scanStatus));
        } else {
            scanInfoJsonObj.set("ScanStatus", static_cast<int>(scanInfo.scanStatus));
            scanInfoJsonObj.set("ScanBeginTime",
                                Poco::DateTimeFormatter::format(scanInfo.scanBeginTime, "%Y-%m-%d %H:%M:%S"));
            scanInfoJsonObj.set("ScanEndTime",
                                Poco::DateTimeFormatter::format(scanInfo.scanEndTime, "%Y-%m-%d %H:%M:%S"));
            scanInfoJsonObj.set("TotalVideos", static_cast<std::size_t>(m_videoInfos.at(scanInfoPair.first).size()));
        }
        outJsonArr.add(scanInfoJsonObj);
    }

    outJsonArr.stringify(out);
}

void ApiManager::List(const Poco::JSON::Object &param, std::ostream &out)
{
    if (param.isNull("videoType")) {
        out << R"({"success": false, "msg": "Video type is not given!"})";
        return;
    }

    auto findResult = STR_TO_VIDEO_TYPE.find(param.get("videoType"));
    if (findResult == STR_TO_VIDEO_TYPE.end()) {
        out << R"({"success": false, "msg": "Video type is invalid!"})";
        return;
    }
    VideoType videoType = findResult->second;

    enum VideoStatus {
        INCOMPLETE,
        COMPLETE,
        ALL,
    };

    static std::map<std::string, VideoStatus> videoStatusStrToEnum = {
        {"incomplete", INCOMPLETE},
        {"complete", COMPLETE},
        {"all", ALL},
    };

    VideoStatus videoStatus;
    if (param.isNull("status")) {
        videoStatus = ALL;
    } else {
        const std::string videoStatusStr = Poco::toLower(param.getValue<std::string>("status"));
        auto iter = videoStatusStrToEnum.find(videoStatusStr);
        if (iter == videoStatusStrToEnum.end()) {
            out << R"({"success": false, "msg": "Video status is invalid!"})";
            return;
        } else {
            videoStatus = iter->second;
        }
    }

    Poco::JSON::Array           outJsonArr;
    std::unique_lock<std::mutex> locker(m_scanInfos.at(videoType).lock, std::try_to_lock);
    if (!locker.owns_lock()) {
        out << R"({"success": false, "msg": "Scanning/refreshing job is still unfinished!"})";
        return;
    } else if (m_scanInfos[videoType].scanStatus == NEVER_SCANNED) {
        out << R"({"success": false, "msg": "The datasource has never been scanned, please scan first!"})";
        return;
    } else {
        for (std::size_t i = 0; i < m_videoInfos.at(videoType).size(); i++) {
            const auto& videoInfo = m_videoInfos.at(videoType).at(i);
            Poco::JSON::Object jsonObj;
            switch (videoStatus) {
                case INCOMPLETE: {
                    if (videoInfo.nfoStatus == NFO_FORMAT_MATCH && videoInfo.posterStatus == POSTER_COMPELETED) {
                        continue;
                    }
                    break;
                }
                case COMPLETE: {
                    if (videoInfo.nfoStatus != NFO_FORMAT_MATCH || videoInfo.posterStatus != POSTER_COMPELETED) {
                        continue;
                        ;
                    }
                    break;
                }
                case ALL:
                default:
                    break;
            }
            jsonObj.set("id", i);
            VideoInfoToBriefJson(videoInfo, jsonObj);
            outJsonArr.add(jsonObj);
        }
    }

    Poco::JSON::Object outJsonObj;
    outJsonObj.set("success", "true");
    outJsonObj.set("list", outJsonArr);
    outJsonObj.stringify(out);
}

void ApiManager::Detail(const Poco::JSON::Object &param, std::ostream &out)
{
    if (param.isNull("videoType")) {
        out << R"({"success": false, "msg": "Video type is not given!"})";
        return;
    }

    if (param.isNull("id")) {
        out << R"({"success": false, "msg": "Id is not given!"})";
        return;
    }

    auto findResult = STR_TO_VIDEO_TYPE.find(param.get("videoType"));
    if (findResult == STR_TO_VIDEO_TYPE.end()) {
        out << R"({"success": false, "msg": "Video type is invalid!"})";
        return;
    }
    VideoType videoType = findResult->second;

    Poco::JSON::Object outJsonObj;
    std::unique_lock<std::mutex> locker(m_scanInfos.at(videoType).lock, std::try_to_lock);
    if (locker.owns_lock() && m_scanInfos[videoType].scanStatus != NEVER_SCANNED) {
        size_t id = std::stoull(param.getValue<std::string>("id"));
        if (id >= m_videoInfos.at(videoType).size()) { // 防止ID越界
            out << R"({"success": false, "msg": "Id is out of range!"})";
            return;
        }
        outJsonObj.set("id", id);
        VideoInfoToDetailedJson(m_videoInfos.at(videoType).at(id), outJsonObj);
    }

    outJsonObj.stringify(out);
}

void ApiManager::Scrape(const Poco::JSON::Object &param, std::ostream &out)
{
    if (param.isNull("videoType")) {
        out << R"({"success": false, "msg": "Video type is not given!"})";
        return;
    }

    if (param.isNull("id")) {
        out << R"({"success": false, "msg": "Id is not given!"})";
        return;
    }

    if (param.isNull("tmdbid")) {
        out << R"({"success": false, "msg": "TMDB id is not given!"})";
        return;
    }

    auto findResult = STR_TO_VIDEO_TYPE.find(param.get("videoType"));
    if (findResult == STR_TO_VIDEO_TYPE.end()) {
        out << R"({"success": false, "msg": "Video type is invalid!"})";
        return;
    }
    VideoType videoType = findResult->second;

    std::unique_lock<std::mutex> locker(m_scanInfos.at(videoType).lock, std::try_to_lock);
    if (!locker.owns_lock()) {
        out << R"({"success": false, "msg": "Still scanning in background!"})";
        return;
    }

    if (m_scanInfos[videoType].scanStatus == NEVER_SCANNED) {
        out << R"({"success": false, "msg": "Never scanned!"})";
        return;
    }

    size_t id = std::stoull(param.getValue<std::string>("id"));
    if (id >= m_videoInfos.at(videoType).size()) { // 防止ID越界
        out << R"({"success": false, "msg": "Id is out of range!"})";
        return;
    }

    // 电视剧需要给定季编号
    int seasonId = 0;
    if (videoType == TV) {
        if (!param.has("seasonId")) {
            out << R"({"success": false, "msg": "TV should give season id!"})";
            return;
        }
        seasonId = std::stoi(param.getValue<std::string>("seasonId"));
    }

    auto &videoInfo                = m_videoInfos.at(videoType).at(id);
    videoInfo.videoDetail.uniqueid = std::stoi(param.getValue<std::string>("tmdbid"));
    std::stringstream sS;
    switch (videoType) {
        case MOVIE: {
            ScrapeMovie(videoInfo, out);
            return;
        }
        case TV: {
            ScrapeTV(videoInfo, out, seasonId);
            return;
        }
        default:
            out << R"({"success": false, "msg": "Unsupported video type!"})";
            return;
    }
}

void ApiManager::ProcessRefresh(VideoType videoType)
{
    ProcessScan(videoType);

    std::unique_lock<std::mutex> locker(m_refreshInfos[videoType].lock, std::try_to_lock);
    m_refreshInfos[videoType].refreshStatus = REFRESHING;
    m_refreshInfos[videoType].refreshBeginTime = Poco::DateTime();
    if (videoType == MOVIE) {
        RefreshMovie();
    } else if (videoType == TV) {
        RefreshTV();
    }
    m_refreshInfos[videoType].refreshEndTime = Poco::DateTime();
    m_refreshInfos[videoType].refreshStatus  = REFRESHING_FINISHED;
}

void ApiManager::Refresh(const Poco::JSON::Object &param, std::ostream &out)
{
    if (param.isNull("videoType")) {
        out << R"({"success": false, "msg": "Video type is not given!"})";
        return;
    }

    auto findResult = STR_TO_VIDEO_TYPE.find(param.get("videoType"));
    if (findResult == STR_TO_VIDEO_TYPE.end()) {
        out << R"({"success": false, "msg": "Video type is invalid!"})";
        return;
    }
    VideoType videoType = findResult->second;

    // 无法加锁说明后台正在扫描
    std::unique_lock<std::mutex> locker(m_scanInfos[videoType].lock, std::try_to_lock);
    if (!locker.owns_lock()) {
        out << R"({"success": false, "msg": "Still refreshing!"})";
        return;
    }

    // 加锁后在新线程进行扫描
    std::thread scanThread(std::bind(&ApiManager::ProcessRefresh, this, std::placeholders::_1), videoType);
    scanThread.detach();

    out << R"({"success": true})";
}

void ApiManager::RefreshResult(const Poco::JSON::Object &, std::ostream &out)
{
    Poco::JSON::Array outJsonArr;
    for (auto &refreshInfoPair : m_refreshInfos) {
        Poco::JSON::Object refreshInfoJsonObj;
        refreshInfoJsonObj.set("VideoType", VIDEO_TYPE_TO_STR.at(refreshInfoPair.first));
        auto                        &refreshInfo = refreshInfoPair.second;
        std::unique_lock<std::mutex> locker(refreshInfo.lock, std::try_to_lock);
        if (!locker.owns_lock()) { // 避免原子性问题
            refreshInfoJsonObj.set("RefreshStatus", static_cast<int>(REFRESHING));
            refreshInfoJsonObj.set("RefreshBeginTime",
                                   Poco::DateTimeFormatter::format(refreshInfo.refreshBeginTime, "%Y-%m-%d %H:%M:%S"));
        } else if (refreshInfo.refreshStatus == NEVER_REFRESHED) {
            refreshInfoJsonObj.set("RefreshStatus", static_cast<int>(refreshInfo.refreshStatus));
        } else {
            refreshInfoJsonObj.set("RefreshStatus", static_cast<int>(refreshInfo.refreshStatus));
            refreshInfoJsonObj.set("RefreshBeginTime",
                                   Poco::DateTimeFormatter::format(refreshInfo.refreshBeginTime, "%Y-%m-%d %H:%M:%S"));
            refreshInfoJsonObj.set("RefreshEndTime",
                                   Poco::DateTimeFormatter::format(refreshInfo.refreshEndTime, "%Y-%m-%d %H:%M:%S"));
            refreshInfoJsonObj.set("TotalVideos",
                                   static_cast<std::size_t>(m_videoInfos.at(refreshInfoPair.first).size()));
        }
        outJsonArr.add(refreshInfoJsonObj);
    }

    outJsonArr.stringify(out);
}

void ApiManager::AutoUpdateTV()
{
    if (m_videoInfos.at(TV).empty()) {
        LOG_WARN("Empty tv show in datasource, scan first or add new!");
        return;
    }

    std::unique_lock<std::mutex> locker(m_scanInfos.at(TV).lock, std::try_to_lock);

    LOG_DEBUG("Search for new episodes...");
    for (auto &videoInfo : m_videoInfos.at(TV)) {
        if (!videoInfo.videoDetail.isEnded &&
            videoInfo.videoDetail.episodeNfoCount != videoInfo.videoDetail.episodePaths.size()) {
            LOG_INFO("Try to auto update tv info {}...", videoInfo.videoPath);
            if (!UpdateTV(videoInfo)) {
                LOG_ERROR("Failed to update TV: {}", videoInfo.videoPath);
            }
        }
    }
    LOG_DEBUG("Search finished.");
}

void ApiManager::RefreshMovie()
{
    if (m_videoInfos.at(MOVIE).empty()) {
        LOG_WARN("Empty movie in datasource, scan first or add new!");
        return;
    }

    std::unique_lock<std::mutex> locker(m_scanInfos.at(MOVIE).lock, std::try_to_lock);

    LOG_DEBUG("Refreshing movie nfos...");
    std::vector<std::string> failedVec;
    std::vector<std::string> nfoMisVec;
    int successCount = 0;
    int failedCount = 0;
    int nfoMisCount = 0;

    for (auto &videoInfo : m_videoInfos.at(MOVIE)) {
        if (videoInfo.nfoStatus != NFO_FORMAT_MATCH) {
            LOG_DEBUG("Nfo file incorrect, skipped: {}", videoInfo.videoPath);
            nfoMisCount++;
            nfoMisVec.push_back(videoInfo.videoPath);
            continue;
        }

        // TODO: API接口调整
        if (!ScrapeMovie(videoInfo, std::cout)) {
            failedVec.push_back(videoInfo.videoPath);
            continue;
        }
        successCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        LOG_INFO("Progress:\t{}/{}", successCount + failedCount + nfoMisCount, m_videoInfos.at(MOVIE).size());
    }

    printf("Movie refresh summary:\n\tTotal: %lu\n\tSuccess: %d\n\tFailed: %d\n\tNfoMis: %d\n",
           m_videoInfos.at(MOVIE).size(),
           successCount,
           failedCount,
           nfoMisCount);
    printf("Failed videos:\n");
    for (auto v : failedVec) {
        printf("\t%s\n", v.c_str());
    }
    printf("NfoMis videos:\n");
    for (auto v : nfoMisVec) {
        printf("\t%s\n", v.c_str());
    }
    LOG_DEBUG("Refresh movie nfos finished.");
}

void ApiManager::RefreshTV()
{
    if (m_videoInfos.at(TV).empty()) {
        LOG_WARN("Empty tv show in datasource, scan first or add new!");
        return;
    }

    std::unique_lock<std::mutex> locker(m_scanInfos.at(TV).lock, std::try_to_lock);

    LOG_DEBUG("Refreshing tv nfos...");
    std::vector<std::string> failedVec;
    std::vector<std::string> nfoMisVec;
    int                      successCount = 0;
    int                      failedCount  = 0;
    int                      nfoMisCount  = 0;

    for (auto &videoInfo : m_videoInfos.at(TV)) {
        if (videoInfo.nfoStatus != NFO_FORMAT_MATCH) {
            LOG_DEBUG("Nfo file incorrect, skipped: {}", videoInfo.videoPath);
            nfoMisCount++;
            nfoMisVec.push_back(videoInfo.videoPath);
            continue;
        }

        LOG_DEBUG("Refreshing TV nfo: {}", videoInfo.videoPath);

        // TODO: API接口调整
        if (!ScrapeTV(videoInfo, std::cout, videoInfo.videoDetail.seasonNumber)) {
            failedVec.push_back(videoInfo.videoPath);
            continue;
        }

        successCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        LOG_INFO("\nProgress:\t{}/{}", successCount + failedCount + nfoMisCount, m_videoInfos.at(TV).size());
    }

    printf("TV refresh summary:\n\tTotal: %lu\n\tSuccess: %d\n\tFailed: %d\n\tNfoMis: %d\n",
           m_videoInfos.at(TV).size(),
           successCount,
           failedCount,
           nfoMisCount);
    if (!failedVec.empty()) {
        printf("Failed videos:\n");
        for (auto v : failedVec) {
            printf("\t%s\n", v.c_str());
        }
    }
    if (!nfoMisVec.empty()) {
        printf("NfoMis videos:\n");
        for (auto v : nfoMisVec) {
            printf("\t%s\n", v.c_str());
        }
    }
    LOG_DEBUG("Refresh tv nfos finished.");
}

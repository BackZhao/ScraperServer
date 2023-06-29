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
            scanInfoJsonObj.set("ScanStatus", static_cast<int>(NEVER_SCANNED));
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

    Poco::JSON::Array           outJsonArr;
    std::unique_lock<std::mutex> locker(m_scanInfos.at(videoType).lock, std::try_to_lock);
    if (locker.owns_lock() && m_scanInfos[videoType].scanStatus != NEVER_SCANNED) {
        uint32_t id = 0;
        for (const auto &videoInfo : m_videoInfos.at(videoType)) {
            Poco::JSON::Object jsonObj;
            VideoInfoToBriefJson(id++, videoInfo, jsonObj);
            outJsonArr.add(jsonObj);
        }
    }

    outJsonArr.stringify(out);
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
        VideoInfoToDetailedJson(id, m_videoInfos.at(videoType).at(id), outJsonObj);
    }

    outJsonObj.stringify(out);
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

    for (auto &oldVideoInfo : m_videoInfos.at(MOVIE)) {
        if (oldVideoInfo.nfoStatus != NFO_FORMAT_MATCH) {
            LOG_DEBUG("Nfo file incorrect, skipped: {}", oldVideoInfo.videoPath);
            nfoMisCount++;
            nfoMisVec.push_back(oldVideoInfo.videoPath);
            continue;
        }

        // TODO: 后续取消XML全解析即可优化此处
        VideoInfo videoInfo(oldVideoInfo.videoType, oldVideoInfo.videoPath);
        videoInfo.nfoPath = oldVideoInfo.nfoPath;
        videoInfo.videoDetail.uniqueid = oldVideoInfo.videoDetail.uniqueid;

        LOG_DEBUG("Refreshing movie nfo: {}", videoInfo.videoPath);

        std::stringstream sS;
        if (!GetMovieDetail(sS, videoInfo.videoDetail.uniqueid)) {
            failedCount++;
            failedVec.push_back(videoInfo.videoPath);
            continue;
        }
        if (!ParseMovieDetailsToVideoDetail(sS, videoInfo.videoDetail)) {
            failedCount++;
            failedVec.push_back(videoInfo.videoPath);
            continue;
        }
        if (!GetMovieCredits(sS, videoInfo.videoDetail.uniqueid)) {
            failedCount++;
            failedVec.push_back(videoInfo.videoPath);
            continue;
        }
        if (!ParseCreditsToVideoDetail(sS, videoInfo.videoDetail)) {
            failedCount++;
            failedVec.push_back(videoInfo.videoPath);
            continue;
        }
        if (!VideoInfoToNfo(videoInfo, videoInfo.nfoPath)) {
            failedCount++;
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
}

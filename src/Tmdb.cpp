#include "Tmdb.h"

#include <fstream>

#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include <Poco/JSON/Object.h>
#include <Poco/URI.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>

#include "Config.h"
#include "DataConvert.h"
#include "Logger.h"

using namespace Poco::JSON;

static std::size_t ReplaceString(std::string& inout, const std::string& what, const std::string& with)
{
    std::size_t count{};
    for (std::string::size_type pos{}; inout.npos != (pos = inout.find(what.data(), pos, what.length()));
         pos += with.length(), ++count) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
}

bool SendRequest(std::ostream& out, Poco::URI& uri)
{
    Poco::Net::HTTPRequest       request;
    Poco::Net::HTTPResponse      response;
    Poco::Net::HTTPClientSession session;

    // 设置访问的各项参数
    std::string path(uri.getPathAndQuery());
    if (path.empty()) {
        path = "/";
    }
    session.setHost(uri.getHost());
    session.setPort(uri.getPort());
    request.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
    request.setURI(path);
    request.setVersion(Poco::Net::HTTPMessage::HTTP_1_1);

    try {
        session.sendRequest(request);
        auto& rs = session.receiveResponse(response);
        // 响应码必须为200
        if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
            LOG_ERROR("Response status code is {} for {}", response.getStatus(), uri.toString());
            std::string responseStr;
            Poco::StreamCopier::copyToString(rs, responseStr);
            LOG_ERROR("Response content is:\n{}", responseStr);
            return false;
        } else {
            Poco::StreamCopier::copyStream(rs, out);
        }
    } catch (Poco::Exception& e) {
        LOG_ERROR("Send request failed for uri {} : ", e.displayText());
        return false;
    }

    return true;
}

bool Search(std::ostream& out, VideoType VideoType, const std::string& keywords, const std::string& year)
{
    // 拼接访问的URL
    ApiUrlType apiUrlType = VideoType == TV ? SEARCH_TV : SEARCH_MOVIE;
    std::string uriStr        = Config::Instance().GetApiUrl(apiUrlType);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    uri.addQueryParameter("query", keywords);
    if (!year.empty()) {
        uri.addQueryParameter("year", year);
    }
    LOG_DEBUG("Search uri is: {}", uri.toString());

    return SendRequest(out, uri);
}

bool GetMovieDetail(std::ostream& out, int tmdbId)
{
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_MOVIE_DETAIL) + std::to_string(tmdbId);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get movie detail uri is: {}", uri.toString());

    return SendRequest(out, uri);
}

bool GetTVDetail(std::ostream& out, int tmdbId)
{
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_TV_DETAIL) + std::to_string(tmdbId);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get tv detail uri is: {}", uri.toString());

    return SendRequest(out, uri);
}

bool GetSeasonDetail(std::ostream& out, int tmdbId, int seasonNum)
{
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_SEASON_DETAIL);
    if (ReplaceString(uriStr, "{tv_id}", std::to_string(tmdbId)) <= 0) {
        LOG_ERROR("Invalid api format for geting season detail(need contain {tv_id}): {}", uriStr);
        return false;
    }
    uriStr += std::to_string(seasonNum);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get season detail uri is: {}", uri.toString());

    return SendRequest(out, uri);
}

bool GetMovieCredits(std::ostream& out, int tmdbId)
{
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_MOVIE_CREDITS);
    if (ReplaceString(uriStr, "{movie_id}", std::to_string(tmdbId)) <= 0) {
        LOG_ERROR("Invalid api format for geting movie credits(need contain {movie_id}): {}", uriStr);
        return false;
    }

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get movie credits uri is: {}", uri.toString());

    return SendRequest(out, uri);
}

bool GetTVCredits(std::ostream& out, int tmdbId)
{
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_TV_CREDITS);
    if (ReplaceString(uriStr, "{tv_id}", std::to_string(tmdbId)) <= 0) {
        LOG_ERROR("Invalid api format for geting tv credits(need contain {movie_id}): {}", uriStr);
        return false;
    }

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get tv credits uri is: {}", uri.toString());

    return SendRequest(out, uri);
}

bool DownloadPoster(VideoInfo& videoInfo)
{
    std::ofstream ofs(videoInfo.posterPath);
    if (!ofs.is_open()) {
        LOG_ERROR("Failed to open file {} to write poster!", videoInfo.posterPath);
        return false;
    }

    if (videoInfo.videoDetail.posterUrl.empty()) {
        LOG_ERROR("Poster uri for {} is empty!", videoInfo.videoPath);
        return false;
    } else {
        LOG_DEBUG("Download poster {} for {}", videoInfo.videoDetail.posterUrl, videoInfo.videoPath);
        Poco::URI uri(videoInfo.videoDetail.posterUrl);
        return SendRequest(ofs, uri);
    }
}

bool UpdateTV(VideoInfo& videoInfo)
{
    if (videoInfo.videoDetail.isEnded) {
        LOG_ERROR("Current video is ended: {}", videoInfo.videoPath);
        return false;
    }

    std::stringstream seasonDetailStream;
    if (!GetSeasonDetail(seasonDetailStream, videoInfo.videoDetail.uniqueid, videoInfo.videoDetail.seasonNumber)) {
        LOG_ERROR("Get season detail failed for {}", videoInfo.videoPath);
        return false;
    }

    Parser      parser;
    Object::Ptr jsonPtr = nullptr;
    try {
        auto result = parser.parse(seasonDetailStream);
        jsonPtr     = result.extract<Object::Ptr>();
    } catch (Poco::Exception& e) {
        LOG_ERROR("Parse season detail failed, text: {}", seasonDetailStream.str());
        return false;
    }

    // TODO: 当TMDB API剧集数量尚未更新时, 使用默认标题
    auto episodesJsonArr = jsonPtr->getArray("episodes");
    if (episodesJsonArr->size() < videoInfo.videoDetail.episodePaths.size()) {
        LOG_ERROR(
            "TMDB api return less episode: {} < {}", episodesJsonArr->size(), videoInfo.videoDetail.episodePaths.size());
        return false;
    }

    if (!WriteEpisodeNfo(episodesJsonArr, videoInfo.videoDetail.episodePaths, videoInfo.videoDetail.seasonNumber)) {
        LOG_ERROR("Write episode nfos failed!");
        return false;
    } else {
        // 如果写入成功, 需要即时更新, 否则剧集nfo个数未更新会导致反复写入新剧集的nfo
        videoInfo.videoDetail.episodeNfoCount = videoInfo.videoDetail.episodePaths.size();
    }

    // FIXME: XML更新错误 
    // std::stringstream tvDetailStream;
    // if (!GetTvDetail(tvDetailStream, videoInfo.videoDetail.uniqueid)) {
    //     LOG_ERROR("Get tv detail failed for {}", videoInfo.videoPath);
    //     return false;
    // }
    // result  = parser.parse(tvDetailStream);
    // jsonPtr = result.extract<Object::Ptr>();
    // if (jsonPtr->getValue<std::string>("status").compare("Ended")) {
    //     if (SetTVEnded(videoInfo.nfoPath)) {
    //         LOG_ERROR("TV {} is set to ended failed!", videoInfo.videoPath);
    //     } else {
    //         LOG_INFO("TV {} is set to ended!", videoInfo.videoPath);
    //     }
    // }

    return true;
}

bool ScrapeMovie(VideoInfo& videoInfo, std::ostream& out)
{
    videoInfo.videoDetail.genre.clear();
    videoInfo.videoDetail.countries.clear();
    videoInfo.videoDetail.credits.clear();
    videoInfo.videoDetail.studio.clear();
    videoInfo.videoDetail.actors.clear();

    std::stringstream sS;
    if (!GetMovieDetail(sS, videoInfo.videoDetail.uniqueid)) {
        out << R"({"success": false, "msg": "Get movie detail failed!"})";
        return false;
    }
    if (!ParseMovieDetailsToVideoDetail(sS, videoInfo.videoDetail)) {
        out << R"({"success": false, "msg": "Parse movie detail failed!"})";
        return false;
    }
    if (!GetMovieCredits(sS, videoInfo.videoDetail.uniqueid)) {
        out << R"({"success": false, "msg": "Get movie credits failed!"})";
        return false;
    }
    if (!ParseCreditsToVideoDetail(sS, videoInfo.videoDetail)) {
        out << R"({"success": false, "msg": "Parse movie credits failed!"})";
        return false;
    }
    if (!VideoInfoToNfo(videoInfo, videoInfo.nfoPath, true)) {
        out << R"({"success": false, "msg": "Write nfo failed!"})";
        return false;
    }
    if (!DownloadPoster(videoInfo)) {
        out << R"({"success": false, "msg": "Download poster failed!"})";
        return false;
    }

    videoInfo.posterStatus = POSTER_COMPELETED;
    videoInfo.nfoStatus = NFO_FORMAT_MATCH;

    Object videoInfoJsonObj;
    VideoInfoToDetailedJson(videoInfo, videoInfoJsonObj);

    Object outJson;
    outJson.set("success", true);
    outJson.set("videoInfo", videoInfoJsonObj);

    outJson.stringify(out);
    return true;
}

bool ScrapeTV(VideoInfo& videoInfo, std::ostream& out, int seasonId)
{
    videoInfo.videoDetail.genre.clear();
    videoInfo.videoDetail.countries.clear();
    videoInfo.videoDetail.credits.clear();
    videoInfo.videoDetail.studio.clear();
    videoInfo.videoDetail.actors.clear();

    std::stringstream sS;
    if (!GetTVDetail(sS, videoInfo.videoDetail.uniqueid)) {
        out << R"({"success": false, "msg": "Get tv detail failed!"})";
        return false;
    }

    if (!ParseTVDetailsToVideoDetail(sS, videoInfo.videoDetail, seasonId)) {
        out << R"({"success": false, "msg": "Parse tv detail failed!"})";
        return false;
    }

    if (!GetSeasonDetail(sS, videoInfo.videoDetail.uniqueid, seasonId)) {
        LOG_ERROR("Get season detail failed for {}", videoInfo.videoPath);
        out << R"({"success": false, "msg": "Get season detail failed!"})";
        return false;
    }

    Parser      parser;
    Object::Ptr jsonPtr = nullptr;
    try {
        auto result = parser.parse(sS);
        jsonPtr     = result.extract<Object::Ptr>();
    } catch (Poco::Exception& e) {
        LOG_ERROR("Parse season detail failed, text: {}", sS.str());
        out << R"({"success": false, "msg": "Parse season detail failed!"})";
        return false;
    }
    auto episodesJsonArr = jsonPtr->getArray("episodes");
    if (!WriteEpisodeNfo(episodesJsonArr, videoInfo.videoDetail.episodePaths, seasonId)) {
        LOG_ERROR("Write episode nfos failed! path: {}", videoInfo.videoPath);
        return false;
    } else {
        // 如果写入成功, 需要即时更新, 否则剧集nfo个数未更新会导致反复写入新剧集的nfo
        videoInfo.videoDetail.episodeNfoCount = videoInfo.videoDetail.episodePaths.size();
    }

    if (!GetTVCredits(sS, videoInfo.videoDetail.uniqueid)) {
        out << R"({"success": false, "msg": "Get tv credits failed!"})";
        return false;
    }

    if (!ParseCreditsToVideoDetail(sS, videoInfo.videoDetail)) {
        out << R"({"success": false, "msg": "Parse tv credits failed!"})";
        return false;
    }

    if (!VideoInfoToNfo(videoInfo, videoInfo.nfoPath, true)) {
        out << R"({"success": false, "msg": "Write nfo failed!"})";
        return false;
    }

    if (!DownloadPoster(videoInfo)) {
        out << R"({"success": false, "msg": "Download poster failed!"})";
        return false;
    }

    videoInfo.nfoStatus = NFO_FORMAT_MATCH;
    videoInfo.posterStatus = POSTER_COMPELETED;

    Object videoInfoJsonObj;
    VideoInfoToDetailedJson(videoInfo, videoInfoJsonObj);

    Object outJson;
    outJson.set("success", true);
    outJson.set("videoInfo", videoInfoJsonObj);
    outJson.stringify(out);

    return true;
}

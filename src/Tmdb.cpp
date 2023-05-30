#include "Tmdb.h"

#include "Config.h"
#include "Logger.h"
#include "DataConvert.h"

#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include <Poco/JSON/Object.h>
#include <Poco/URI.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>

using namespace Poco::JSON;

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

    Poco::Net::HTTPRequest request;
    Poco::Net::HTTPResponse response;
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
    session.sendRequest(request);

    auto& rs = session.receiveResponse(response);
    Poco::StreamCopier::copyStream(rs, out);

    return true;
}

bool GetTvDetail(std::ostream& out, int tmdbId)
{
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_TV_DETAIL) + std::to_string(tmdbId);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get tv detail uri is: {}", uri.toString());

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
    session.sendRequest(request);

    auto& rs = session.receiveResponse(response);
    Poco::StreamCopier::copyStream(rs, out);

    return true;
}

std::size_t ReplaceString(std::string& inout, const std::string& what, const std::string& with)
{
    std::size_t count{};
    for (std::string::size_type pos{}; inout.npos != (pos = inout.find(what.data(), pos, what.length()));
         pos += with.length(), ++count) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
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
    session.sendRequest(request);

    auto& rs = session.receiveResponse(response);
    Poco::StreamCopier::copyStream(rs, out);
    Poco::StreamCopier::copyStream(rs, std::cout);

    return true;
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

    Parser parser;
    auto        result          = parser.parse(seasonDetailStream);
    Object::Ptr jsonPtr = result.extract<Object::Ptr>();
    auto episodesJsonArr = jsonPtr->getArray("episodes");

    // TODO: 当TMDB API剧集数量尚未更新时, 使用默认标题
    if (episodesJsonArr->size() < videoInfo.videoDetail.episodePaths.size()) {
        LOG_ERROR(
            "TMDB api return less episode: {} < {}", episodesJsonArr->size(), videoInfo.videoDetail.episodePaths.size());
        return false;
    }

    if (!WriteEpisodeNfo(episodesJsonArr, videoInfo.videoDetail.episodePaths)) {
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

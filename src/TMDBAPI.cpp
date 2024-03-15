#include "TMDBAPI.h"

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
#include "ISO-3611-1.h"
#include "Logger.h"
#include "Utils.h"

using namespace Poco::JSON;

bool TMDBAPI::IsImagesAllFilled(const VideoDetail& videoDetail)
{
    return (!videoDetail.posterUrl.empty() && !videoDetail.clearLogoUrl.empty() && !videoDetail.fanartUrl.empty());
}

int TMDBAPI::GetLastErrCode()
{
    return m_lastErrCode;
}

const std::string& TMDBAPI::GetLastErrStr()
{
    static std::map<ErrCode, std::string> errMap = {
        {NO_ERR, "No error works, check the server code."},
        {GET_MOVIE_DETAIL_FAILED, "Get movie detail failed."},
        {GET_TV_DETAIL_FAILED, "Get tv detail failed."},
        {GET_SEASON_DETAIL_FAILED, "Get season detail failed."},
        {GET_TV_CREDITS_FAILED, "Get tv credits failed."},
        {GET_MOVIE_CREDITS_FAILED, "Get movie credits failed."},
        {PARSE_MOVIE_DETAIL_FAILED, "Parse movie detail failed."},
        {PARSE_TV_DETAIL_FAILED, "Parse tv detail failed."},
        {PARSE_SEASON_DETAIL_FAILED, "Parse season detail failed."},
        {PARSE_CREDITS_FAILED, "Parse credits failed."},
        {DOWNLOAD_POSTER_FAILED, "Download poster failed."},
        {WRITE_NFO_FILE_FAILED, "Write nfo file failed."},
    };

    return errMap.at(m_lastErrCode);
}

bool TMDBAPI::ParseImagesToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail)
{
    Object::Ptr jsonPtr = nullptr;
    try {
        Parser parser;
        auto   result = parser.parse(sS);
        jsonPtr       = result.extract<Object::Ptr>();
    } catch (Poco::Exception& e) {
        LOG_ERROR("Images json parse failed, text as fllows: ");
        std::cout << sS.str() << std::endl;
        return false;
    }

    // 剧照
    auto backdropsJsonPtr = jsonPtr->getArray("backdrops");
    if (videoDetail.fanartUrl.empty() && !backdropsJsonPtr.isNull() && !backdropsJsonPtr->empty()) {
        videoDetail.fanartUrl = backdropsJsonPtr->getObject(0)->optValue<std::string>("file_path", "");
    }

    // logo
    auto logosJsonPtr = jsonPtr->getArray("logos");
    if (videoDetail.clearLogoUrl.empty() && !logosJsonPtr.isNull() && !logosJsonPtr->empty()) {
        videoDetail.clearLogoUrl = logosJsonPtr->getObject(0)->optValue<std::string>("file_path", "");
    }

    // 海报
    auto postersJsonPtr = jsonPtr->getArray("posters");
    if (videoDetail.posterUrl.empty() && !postersJsonPtr.isNull() && !postersJsonPtr->empty()) {
        videoDetail.posterUrl = postersJsonPtr->getObject(0)->optValue<std::string>("file_path", "");
    }

    return true;
}

bool TMDBAPI::SendRequest(std::ostream& out, const Poco::URI& uri)
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
    if (!Config::Instance().GetHttpProxyHost().empty()) {
        session.setProxy(Config::Instance().GetHttpProxyHost(), Config::Instance().GetHttpProxyPort());
    }

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
        LOG_ERROR("Send request failed for uri {} : {}", uri.toString(), e.displayText());
        return false;
    }

    return true;
}

// bool TMDBAPI::Search(std::ostream& out, VideoType VideoType, const std::string& keywords, const std::string& year)
// {
//     // 拼接访问的URL
//     ApiUrlType apiUrlType = VideoType == TV ? SEARCH_TV : SEARCH_MOVIE;
//     std::string uriStr        = Config::Instance().GetApiUrl(apiUrlType);

//     Poco::URI uri(uriStr);
//     uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
//     uri.addQueryParameter("language", "zh-cn");
//     uri.addQueryParameter("query", keywords);
//     if (!year.empty()) {
//         uri.addQueryParameter("year", year);
//     }
//     LOG_DEBUG("Search uri is: {}", uri.toString());

//     return SendRequest(out, uri);
// }

bool TMDBAPI::ParseMovieDetailsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail)
{
    Object::Ptr jsonPtr = nullptr;
    try {
        Parser parser;
        auto   result = parser.parse(sS);
        jsonPtr       = result.extract<Object::Ptr>();
    } catch (Poco::Exception& e) {
        LOG_ERROR("Movie detail json parse failed, text as fllows: ");
        std::cout << sS.str() << std::endl;
    }

    videoDetail.title          = jsonPtr->getValue<std::string>("title");
    videoDetail.originaltitle  = jsonPtr->getValue<std::string>("original_title");
    videoDetail.ratings.rating = jsonPtr->getValue<double>("vote_average");
    videoDetail.ratings.votes  = jsonPtr->getValue<int>("vote_count");
    videoDetail.plot           = jsonPtr->getValue<std::string>("overview");

    if (!jsonPtr->getValue<std::string>("poster_path").empty()) {
        const std::string imageUrl =
            Config::Instance().GetApiUrl(IMAGE_DOWNLOAD) + Config::Instance().GetImageDownloadQuality();
        videoDetail.posterUrl = imageUrl + jsonPtr->getValue<std::string>("poster_path");
    }

    auto genreJsonPtr = jsonPtr->getArray("genres");
    for (std::size_t i = 0; i < genreJsonPtr->size(); i++) {
        videoDetail.genre.push_back(genreJsonPtr->getObject(i)->getValue<std::string>("name"));
    }

    auto contriesJsonPtr = jsonPtr->getArray("production_countries");
    for (std::size_t i = 0; i < contriesJsonPtr->size(); i++) {
        const std::string& code = contriesJsonPtr->getObject(i)->getValue<std::string>("iso_3166_1");
        videoDetail.countries.push_back(ISO_3611_CODE_TO_STR.at(code));
    }

    videoDetail.premiered = jsonPtr->getValue<std::string>("release_date");

    auto studioJsonArrPtr = jsonPtr->getArray("production_companies");
    for (std::size_t i = 0; i < studioJsonArrPtr->size(); i++) {
        videoDetail.studio.push_back(studioJsonArrPtr->getObject(i)->getValue<std::string>("name"));
    }

    return true;
}

bool TMDBAPI::GetMovieDetail(int tmdbId, VideoDetail& videoDetail)
{
    std::stringstream sS;
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_MOVIE_DETAIL) + std::to_string(tmdbId);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get movie detail uri is: {}", uri.toString());

    if (SendRequest(sS, uri)) {
        return ParseMovieDetailsToVideoDetail(sS, videoDetail);
    } else {
        return false;
    }
}

bool TMDBAPI::ParseTVDetailsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail, int seasonId)
{
    Object::Ptr jsonPtr = nullptr;
    try {
        Parser parser;
        auto   result = parser.parse(sS);
        jsonPtr       = result.extract<Object::Ptr>();
    } catch (Poco::Exception& e) {
        LOG_ERROR("TV detail json parse failed, text as fllows: ");
        std::cout << sS.str() << std::endl;
        return false;
    }

    // 获取指定的季
    Object::Ptr selectedSeason = nullptr;
    auto seasonArr = jsonPtr->getArray("seasons");
    for (size_t i = 0; i < seasonArr->size(); i++) {
        auto season = seasonArr->getObject(i);
        if (season->getValue<int>("season_number") == seasonId) {
            selectedSeason = season;
            break;
        } 
    }
    if (selectedSeason == nullptr) {
        LOG_ERROR("Could not found given season id!");
        return false;
    }
    static std::set<std::string> seasonOne = {
        "第一部", "第1部", "第 1 部",
        "第一季", "第1季", "第 1 季",
        "SEASON ONE", "SEASON1", "SEASON 1",
    };
    auto iter = seasonOne.find(selectedSeason->getValue<std::string>("name"));
    if ( iter != seasonOne.end() || seasonId == 1 ) { // 第一部不加序号
        videoDetail.title = jsonPtr->getValue<std::string>("name");
    } else {
        videoDetail.title = jsonPtr->getValue<std::string>("name") + std::to_string(seasonId);
    }

    const std::string imageUrl =
        Config::Instance().GetApiUrl(IMAGE_DOWNLOAD) + Config::Instance().GetImageDownloadQuality();
    if (selectedSeason->isNull("poster_path")) {
        if (!jsonPtr->getValue<std::string>("poster_path").empty()) {
            videoDetail.posterUrl = imageUrl + jsonPtr->getValue<std::string>("poster_path");
        }
    } else {
        videoDetail.posterUrl = imageUrl + selectedSeason->getValue<std::string>("poster_path");
    }

    if (selectedSeason->getValue<std::string>("overview").empty()) {
        videoDetail.plot = jsonPtr->getValue<std::string>("overview");
    } else {
        videoDetail.plot = selectedSeason->getValue<std::string>("overview");
    }

    videoDetail.originaltitle  = jsonPtr->getValue<std::string>("original_name");
    videoDetail.ratings.rating = jsonPtr->getValue<double>("vote_average");
    videoDetail.ratings.votes  = jsonPtr->getValue<int>("vote_count");

    auto genreJsonPtr = jsonPtr->getArray("genres");
    for (std::size_t i = 0; i < genreJsonPtr->size(); i++) {
        videoDetail.genre.push_back(genreJsonPtr->getObject(i)->getValue<std::string>("name"));
    }

    auto contriesJsonPtr = jsonPtr->getArray("production_countries");
    for (std::size_t i = 0; i < contriesJsonPtr->size(); i++) {
        const std::string& code = contriesJsonPtr->getObject(i)->getValue<std::string>("iso_3166_1");
        videoDetail.countries.push_back(ISO_3611_CODE_TO_STR.at(code));
    }

    videoDetail.premiered = jsonPtr->getValue<std::string>("first_air_date");

    auto studioJsonArrPtr = jsonPtr->getArray("production_companies");
    for (std::size_t i = 0; i < studioJsonArrPtr->size(); i++) {
        videoDetail.studio.push_back(studioJsonArrPtr->getObject(i)->getValue<std::string>("name"));
    }

    return true;
}

bool TMDBAPI::GetTVDetail(int tmdbId, int seasonId, VideoDetail& videoDetail)
{
    std::stringstream sS;
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_TV_DETAIL) + std::to_string(tmdbId);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get tv detail uri is: {}", uri.toString());

    if (SendRequest(sS, uri)) {
        return ParseTVDetailsToVideoDetail(sS, videoDetail, seasonId);
    } else {
        return false;
    }
}

bool TMDBAPI::GetSeasonDetail(int tmdbId, int seasonId, VideoDetail& videoDetail)
{
    std::stringstream sS;
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_SEASON_DETAIL);
    if (ReplaceString(uriStr, "{tv_id}", std::to_string(tmdbId)) <= 0) {
        LOG_ERROR("Invalid api format for geting season detail(need contain {{tv_id}}): {}", uriStr);
        return false;
    }

    uriStr += std::to_string(seasonId);

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get season detail uri is: {}", uri.toString());

    if (SendRequest(sS, uri)) {
        Parser      parser;
        Object::Ptr jsonPtr = nullptr;
        try {
            auto result = parser.parse(sS);
            jsonPtr     = result.extract<Object::Ptr>();
        } catch (Poco::Exception& e) {
            LOG_ERROR("Parse season detail failed, text: {}", sS.str());
            m_lastErrCode = PARSE_SEASON_DETAIL_FAILED;
            return false;
        }
        auto episodesJsonArr = jsonPtr->getArray("episodes");
        if (!WriteEpisodeNfo(episodesJsonArr, videoDetail.episodePaths, seasonId)) {
            LOG_ERROR("Write episode nfos failed!");
            return false;
        } else {
            // 如果写入成功, 需要即时更新, 否则剧集nfo个数未更新会导致反复写入新剧集的nfo
            videoDetail.episodeNfoCount = videoDetail.episodePaths.size();
        }
        return true;
    } else {
        return false;
    }
}

bool TMDBAPI::ParseCreditsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail)
{
    Object::Ptr jsonPtr = nullptr;
    try {
        Parser parser;
        auto   result = parser.parse(sS);
        jsonPtr       = result.extract<Object::Ptr>();
    } catch (Poco::Exception& e) {
        LOG_ERROR("Credits json parse failed, text as fllows: ");
        std::cout << sS.str() << std::endl;
        return false;
    }

    const std::string imageUrl =
        Config::Instance().GetApiUrl(IMAGE_DOWNLOAD) + Config::Instance().GetImageDownloadQuality();

    auto castJsonArrPtr = jsonPtr->getArray("cast");
    for (std::size_t i = 0; i < castJsonArrPtr->size(); i++) {
        auto              castJsonObjPtr = castJsonArrPtr->getObject(i);
        const std::string department     = castJsonObjPtr->getValue<std::string>("known_for_department");
        if (department == "Acting") {
            ActorDetail actor = {
                castJsonObjPtr->getValue<std::string>("name"),
                castJsonObjPtr->getValue<std::string>("character"),
                castJsonObjPtr->getValue<int>("order"),
                castJsonObjPtr->isNull("profile_path")
                    ? ""
                    : imageUrl + castJsonObjPtr->getValue<std::string>("profile_path"),
            };
            videoDetail.actors.push_back(actor);
        }
    }

    auto crewJsonArrPtr = jsonPtr->getArray("crew");
    for (std::size_t i = 0; i < crewJsonArrPtr->size(); i++) {
        auto crewJsonObjPtr = crewJsonArrPtr->getObject(i);
        if (crewJsonObjPtr->optValue<std::string>("job", "") == "Writer") {
            videoDetail.credits.push_back(crewJsonObjPtr->getValue<std::string>("name"));
        } else if (crewJsonObjPtr->optValue<std::string>("job", "") == "Director") {
            videoDetail.director = crewJsonObjPtr->getValue<std::string>("name");
        }
    }

    return true;
}

bool TMDBAPI::GetMovieCredits(int tmdbId, VideoDetail& videoDetail)
{
    std::stringstream sS;

    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_MOVIE_CREDITS);
    if (ReplaceString(uriStr, "{movie_id}", std::to_string(tmdbId)) <= 0) {
        LOG_ERROR("Invalid api format for geting movie credits(need contain {{movie_id}}): {}", uriStr);
        return false;
    }

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get movie credits uri is: {}", uri.toString());

    if (SendRequest(sS, uri)) {
        return ParseCreditsToVideoDetail(sS, videoDetail);
    } else {
        return false;
    }
}

bool TMDBAPI::GetTVCredits(int tmdbId, VideoDetail& videoDetail)
{
    std::stringstream sS;

    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_TV_CREDITS);
    if (ReplaceString(uriStr, "{tv_id}", std::to_string(tmdbId)) <= 0) {
        LOG_ERROR("Invalid api format for geting tv credits(need contain {{movie_id}}): {}", uriStr);
        return false;
    }

    Poco::URI uri(uriStr);
    uri.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uri.addQueryParameter("language", "zh-cn");
    LOG_DEBUG("Get tv credits uri is: {}", uri.toString());

   if (SendRequest(sS, uri)) {
        return ParseCreditsToVideoDetail(sS, videoDetail);
    } else {
        return false;
    }
}

bool TMDBAPI::DownloadImages(VideoInfo& videoInfo)
{
    auto DownloadToFile = [this](const std::string filePath, const std::string uri) {
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) {
            LOG_ERROR("Failed to open file {} to write poster!", filePath);
            return false;
        }

        const std::string imageUrl =
            Config::Instance().GetApiUrl(IMAGE_DOWNLOAD) + Config::Instance().GetImageDownloadQuality();
        if (uri.empty()) {
            LOG_WARN("Image uri for {} is empty!", filePath);
            return false;
        } else {
            LOG_DEBUG("Download poster {} to {}", uri, filePath);
            return SendRequest(ofs, Poco::URI(imageUrl + uri));
        }
    };

    videoInfo.posterStatus    = DownloadToFile(videoInfo.posterPath, videoInfo.videoDetail.posterUrl)
                                    ? FILE_FORMAT_MATCH
                                    : FILE_FORMAT_MISMATCH;
    videoInfo.fanartStatus    = DownloadToFile(videoInfo.fanartPath, videoInfo.videoDetail.fanartUrl)
                                    ? FILE_FORMAT_MATCH
                                    : FILE_FORMAT_MISMATCH;
    videoInfo.clearlogoStatus = DownloadToFile(videoInfo.clearlogoPath, videoInfo.videoDetail.clearLogoUrl)
                                    ? FILE_FORMAT_MATCH
                                    : FILE_FORMAT_MISMATCH;

    return true;
}

bool TMDBAPI::GetMovieImages(VideoInfo& videoInfo)
{
    std::stringstream sS;
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_MOVIE_IMAGES);
    if (ReplaceString(uriStr, "{movie_id}", std::to_string(videoInfo.videoDetail.uniqueid.at("tmdb"))) <= 0) {
        LOG_ERROR("Invalid api format for geting movie images(need contain {{movie_id}}): {}", uriStr);
        return false;
    }

    // 依次使用"中文"-"英文"-"无语言"来获取图像
    Poco::URI uriLangZh(uriStr);
    uriLangZh.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uriLangZh.addQueryParameter("include_image_language", "zh");
    LOG_DEBUG("Get movie images uri(language zh) is: {}", uriLangZh.toString());

    SendRequest(sS, uriLangZh);
    ParseImagesToVideoDetail(sS, videoInfo.videoDetail);

    if (!IsImagesAllFilled(videoInfo.videoDetail)) {
        Poco::URI uriLangEn(uriStr);
        uriLangEn.addQueryParameter("api_key", Config::Instance().GetApiKey());
        uriLangEn.addQueryParameter("include_image_language", "en");
        LOG_DEBUG("Get movie images uri(language en) is: {}", uriLangEn.toString());

        SendRequest(sS, uriLangEn);
        ParseImagesToVideoDetail(sS, videoInfo.videoDetail);
    }

    if (!IsImagesAllFilled(videoInfo.videoDetail)) {
        Poco::URI uriLangNull(uriStr);
        uriLangNull.addQueryParameter("api_key", Config::Instance().GetApiKey());
        uriLangNull.addQueryParameter("include_image_language", "null");
        LOG_DEBUG("Get movie images uri(language null) is: {}", uriLangNull.toString());

        SendRequest(sS, uriLangNull);
        ParseImagesToVideoDetail(sS, videoInfo.videoDetail);
    }

    return true;
}

bool TMDBAPI::GetSeasonImages(VideoInfo& videoInfo, int seasonNumber)
{
    std::stringstream sS;
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_SEASON_IMAGES);
    if (ReplaceString(uriStr, "{tv_id}", std::to_string(videoInfo.videoDetail.uniqueid.at("tmdb"))) <= 0) {
        // FIXME: [*** LOG ERROR #0001 ***] [2024-01-21 12:30:22] [logger] argument not found [/home/bkzhao/Codes/ScraperServer/src/TMDBAPI.cpp(520)]
        LOG_ERROR("Invalid api format for geting tv images(need contain {{tv_id}}): {}", uriStr);
        return false;
    }
    if (ReplaceString(uriStr, "{season_number}", std::to_string(seasonNumber)) <= 0) {
        LOG_ERROR("Invalid api format for geting tv images(need contain {{season_number}}): {}", uriStr);
        return false;
    }

    // 依次使用"中文"-"英文"-"无语言"来获取图像
    Poco::URI uriLangZh(uriStr);
    uriLangZh.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uriLangZh.addQueryParameter("include_image_language", "zh");
    LOG_DEBUG("Get season images uri(language zh) is: {}", uriLangZh.toString());

    SendRequest(sS, uriLangZh);
    ParseImagesToVideoDetail(sS, videoInfo.videoDetail);

    if (!IsImagesAllFilled(videoInfo.videoDetail)) {
        Poco::URI uriLangEn(uriStr);
        uriLangEn.addQueryParameter("api_key", Config::Instance().GetApiKey());
        uriLangEn.addQueryParameter("include_image_language", "en");
        LOG_DEBUG("Get season images uri(language en) is: {}", uriLangEn.toString());

        SendRequest(sS, uriLangEn);
        ParseImagesToVideoDetail(sS, videoInfo.videoDetail);
    }

    if (!IsImagesAllFilled(videoInfo.videoDetail)) {
        Poco::URI uriLangNull(uriStr);
        uriLangNull.addQueryParameter("api_key", Config::Instance().GetApiKey());
        uriLangNull.addQueryParameter("include_image_language", "null");
        LOG_DEBUG("Get season images uri(language null) is: {}", uriLangNull.toString());

        SendRequest(sS, uriLangNull);
        ParseImagesToVideoDetail(sS, videoInfo.videoDetail);
    }

    return true;
}

bool TMDBAPI::GetTVImages(VideoInfo& videoInfo, int seasonNumber)
{
    // 优先使用季的图片
    if (!GetSeasonImages(videoInfo, seasonNumber)) {
        return false;
    }

    // 如果季的图片完整, 则不需要获取剧的图片
    if (IsImagesAllFilled(videoInfo.videoDetail)) {
        return true;
    }

    std::stringstream sS;
    // 拼接访问的URL
    std::string uriStr = Config::Instance().GetApiUrl(GET_TV_IMAGES);
    if (ReplaceString(uriStr, "{tv_id}", std::to_string(videoInfo.videoDetail.uniqueid.at("tmdb"))) <= 0) {
        LOG_ERROR("Invalid api format for geting tv images(need contain {{tv_id}}): {}", uriStr);
        return false;
    }

    // 依次使用"中文"-"英文"-"无语言"来获取图像
    Poco::URI uriLangZh(uriStr);
    uriLangZh.addQueryParameter("api_key", Config::Instance().GetApiKey());
    uriLangZh.addQueryParameter("include_image_language", "zh");
    LOG_DEBUG("Get season images uri(language zh) is: {}", uriLangZh.toString());

    SendRequest(sS, uriLangZh);
    ParseImagesToVideoDetail(sS, videoInfo.videoDetail);

    if (!IsImagesAllFilled(videoInfo.videoDetail)) {
        Poco::URI uriLangEn(uriStr);
        uriLangEn.addQueryParameter("api_key", Config::Instance().GetApiKey());
        uriLangEn.addQueryParameter("include_image_language", "en");
        LOG_DEBUG("Get season images uri(language en) is: {}", uriLangEn.toString());

        SendRequest(sS, uriLangEn);
        ParseImagesToVideoDetail(sS, videoInfo.videoDetail);
    }

    if (!IsImagesAllFilled(videoInfo.videoDetail)) {
        Poco::URI uriLangNull(uriStr);
        uriLangNull.addQueryParameter("api_key", Config::Instance().GetApiKey());
        uriLangNull.addQueryParameter("include_image_language", "null");
        LOG_DEBUG("Get season images uri(language null) is: {}", uriLangNull.toString());

        SendRequest(sS, uriLangNull);
        ParseImagesToVideoDetail(sS, videoInfo.videoDetail);
    }

    return true;
}

bool TMDBAPI::UpdateTV(VideoInfo& videoInfo)
{
    if (videoInfo.videoDetail.isEnded) {
        LOG_ERROR("Current video is ended: {}", videoInfo.videoPath);
        return false;
    }

    std::stringstream seasonDetailStream;
    if (!GetSeasonDetail(videoInfo.videoDetail.uniqueid.at("tmdb"), videoInfo.videoDetail.seasonNumber, videoInfo.videoDetail)) {
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

bool TMDBAPI::ScrapeMovie(VideoInfo& videoInfo, int movieID)
{
    videoInfo.videoDetail.genre.clear();
    videoInfo.videoDetail.countries.clear();
    videoInfo.videoDetail.credits.clear();
    videoInfo.videoDetail.studio.clear();
    videoInfo.videoDetail.actors.clear();

    std::stringstream sS;
    if (!GetMovieDetail(movieID, videoInfo.videoDetail)) {
        m_lastErrCode = GET_MOVIE_DETAIL_FAILED;
        return false;
    }

    videoInfo.videoDetail.uniqueid["tmdb"] = movieID;

    if (!GetMovieCredits(movieID, videoInfo.videoDetail)) {
        m_lastErrCode = GET_MOVIE_CREDITS_FAILED;
        return false;
    }

    if (!VideoInfoToNfo(videoInfo, videoInfo.nfoPath, true, "tmdb")) {
        m_lastErrCode = WRITE_NFO_FILE_FAILED;
        return false;
    }

    if (!GetMovieImages(videoInfo)) {
        m_lastErrCode = DOWNLOAD_POSTER_FAILED;
        return false;
    }

    if (!DownloadImages(videoInfo)) {
        m_lastErrCode = DOWNLOAD_POSTER_FAILED;
        return false;
    }

    videoInfo.nfoStatus = FILE_FORMAT_MATCH;

    LOG_DEBUG("Scrape movie finished, path: {}", videoInfo.videoPath);

    return true;
}

bool TMDBAPI::ScrapeTV(VideoInfo& videoInfo, int tvId, int seasonId)
{
    // 清空所有的矢量, 刮削时矢量元素的添加均为push_back()
    videoInfo.videoDetail.genre.clear();
    videoInfo.videoDetail.countries.clear();
    videoInfo.videoDetail.credits.clear();
    videoInfo.videoDetail.studio.clear();
    videoInfo.videoDetail.actors.clear();
    // 设置季编号
    videoInfo.videoDetail.seasonNumber = seasonId;

    std::stringstream sS;
    if (!GetTVDetail(tvId, seasonId, videoInfo.videoDetail)) {
        m_lastErrCode = GET_TV_DETAIL_FAILED;
        return false;
    }

    videoInfo.videoDetail.uniqueid["tmdb"] = tvId;

    if (!GetSeasonDetail(tvId, seasonId, videoInfo.videoDetail)) {
        m_lastErrCode = GET_SEASON_DETAIL_FAILED;
        return false;
    }

    if (!GetTVCredits(tvId, videoInfo.videoDetail)) {
        m_lastErrCode = GET_TV_CREDITS_FAILED;
        return false;
    }

    if (!VideoInfoToNfo(videoInfo, videoInfo.nfoPath, true, "tmdb")) {
        m_lastErrCode = WRITE_NFO_FILE_FAILED;
        return false;
    }

    if (!GetTVImages(videoInfo, seasonId)) {
        return false;
    }

    if (!DownloadImages(videoInfo)) {
        m_lastErrCode = DOWNLOAD_POSTER_FAILED;
        return false;
    }

    videoInfo.nfoStatus = FILE_FORMAT_MATCH;

    LOG_DEBUG("Scrape tv finished, path: {}", videoInfo.videoPath);

    return true;
}

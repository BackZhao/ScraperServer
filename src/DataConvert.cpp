#include "DataConvert.h"

#include "Config.h"
#include "Logger.h"
#include "iso-3611-1.h"

#include <fstream>

#include <Poco/AutoPtr.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/DOMWriter.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/NodeList.h>
#include <Poco/DOM/Text.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Path.h>
#include <Poco/StreamCopier.h>
#include <Poco/XML/XMLWriter.h>

using Poco::AutoPtr;
using namespace Poco::XML;
using namespace Poco::JSON;

void VideoInfoToBriefJson(uint32_t id, const VideoInfo& videoInfo, Object& outJson)
{
    outJson.set("Id", id);
    outJson.set("VideoType", VIDEO_TYPE_TO_STR.at(videoInfo.videoType));
    outJson.set("VideoPath", videoInfo.videoPath);
    outJson.set("NfoStatus", static_cast<int>(videoInfo.nfoStatus));
    outJson.set("PosterStatus", static_cast<int>(videoInfo.posterStatus));
    outJson.set("NfoPath", videoInfo.nfoPath);
    outJson.set("PosterPath", videoInfo.posterPath);
}

void VideoInfoToDetailedJson(uint32_t id, const VideoInfo& videoInfo, Object& outJson)
{
    VideoInfoToBriefJson(id, videoInfo, outJson);

    // 如果NFO文件格式匹配, 则额外填写NFO的信息
    if (videoInfo.nfoStatus == NFO_FORMAT_MATCH) {
        Object videoDetailJson;
        videoDetailJson.set("Title", videoInfo.videoDetail.title);
        videoDetailJson.set("OriginalTitle", videoInfo.videoDetail.originaltitle);

        Object ratingsJson;
        ratingsJson.set("Rating", videoInfo.videoDetail.ratings.rating);
        ratingsJson.set("Votes", videoInfo.videoDetail.ratings.votes);
        videoDetailJson.set("Ratings", ratingsJson);

        videoDetailJson.set("Plot", videoInfo.videoDetail.plot);
        videoDetailJson.set("Uniqueid", videoInfo.videoDetail.uniqueid);

        Array genreArrJson;
        for (const auto& genre : videoInfo.videoDetail.genre) {
            genreArrJson.add(genre);
        }
        videoDetailJson.set("Genre", genreArrJson);

        Array contriesArrJson;
        for (const auto& country : videoInfo.videoDetail.countries) {
            contriesArrJson.add(country);
        }
        videoDetailJson.set("Countries", contriesArrJson);

        Array creditsArrJson;
        for (const auto& credit : videoInfo.videoDetail.credits) {
            creditsArrJson.add(credit);
        }
        videoDetailJson.set("Credits", creditsArrJson);

        videoDetailJson.set("Director", videoInfo.videoDetail.director);
        videoDetailJson.set("Premiered", videoInfo.videoDetail.premiered);
        videoDetailJson.set("Studio", videoInfo.videoDetail.studio);

        Array actorsArrJson;
        for (const auto& actor : videoInfo.videoDetail.actors) {
            Object actorJson;
            actorJson.set("name", actor.name);
            actorJson.set("role", actor.role);
            // actorJson.set("order", actor.order);
            actorJson.set("thumb", actor.thumb);
            actorsArrJson.add(actorJson);
        }
        videoDetailJson.set("Actors", actorsArrJson);

        // 电视剧额外填写剧集相关的信息
        if (videoInfo.videoType == TV) {
            videoDetailJson.set("SeasonNumber", videoInfo.videoDetail.seasonNumber);
            videoDetailJson.set("Status", videoInfo.videoDetail.isEnded ? "Ended" : "Continuing");
            videoDetailJson.set("EpisodeNfoCount", videoInfo.videoDetail.episodeNfoCount);
            videoDetailJson.set("EpisodeCount", videoInfo.videoDetail.episodePaths.size());
            Array episodePathsArrJson;
            for (const auto& episodePath : videoInfo.videoDetail.episodePaths) {
                episodePathsArrJson.add(episodePath);
            }
            videoDetailJson.set("EpisodePaths", episodePathsArrJson);
        }

        outJson.set("VideoDetail", videoDetailJson);
    }
}

bool VideoInfoToNfo(const VideoInfo& videoInfo, const std::string& nfoPath)
{
    std::ofstream ofs(nfoPath);
    if (!ofs.is_open()) {
        LOG_ERROR("Can't open file {} for write.", nfoPath);
        return false;
    }

    // TODO: 使用XML方式写入process instruction
    ofs << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)" << std::endl;

    static std::map<VideoType, std::string> rootTagStr = {
        {MOVIE, "movie"},
        {TV, "tvshow"},
        {MOVIE_SET, "movie"},
    };

    AutoPtr<Document> dom     = new Document();
    AutoPtr<Element>  rootEle = dom->createElement(rootTagStr.at(videoInfo.videoType));
    dom->appendChild(rootEle);

    auto createAndAppendText = [dom](Element* parent, const std::string& tagName, const std::string& text) {
        AutoPtr<Element> ele      = dom->createElement(tagName);
        AutoPtr<Text>    textNode = dom->createTextNode(text);
        ele->appendChild(textNode);
        parent->appendChild(ele);
    };

    createAndAppendText(rootEle, "title", videoInfo.videoDetail.title);
    createAndAppendText(rootEle, "originaltitle", videoInfo.videoDetail.originaltitle);

    AutoPtr<Element> ratingsEle = dom->createElement("ratings");
    rootEle->appendChild(ratingsEle);
    AutoPtr<Element> ratingEle = dom->createElement("rating");
    ratingEle->setAttribute("max", "10");
    ratingEle->setAttribute("default", "true");
    ratingEle->setAttribute("name", "themoviedb");
    ratingsEle->appendChild(ratingEle);
    createAndAppendText(ratingEle, "value", std::to_string(videoInfo.videoDetail.ratings.rating));
    createAndAppendText(ratingEle, "votes", std::to_string(videoInfo.videoDetail.ratings.votes));

    createAndAppendText(rootEle, "plot", videoInfo.videoDetail.plot);

    AutoPtr<Element> uniqueidEle = dom->createElement("uniqueid");
    uniqueidEle->setAttribute("type", "tmdb");
    uniqueidEle->setAttribute("default", "true");
    AutoPtr<Text> uniqueidText = dom->createTextNode(std::to_string(videoInfo.videoDetail.uniqueid));
    uniqueidEle->appendChild(uniqueidText);
    rootEle->appendChild(uniqueidEle);

    for (const auto& genre : videoInfo.videoDetail.genre) {
        createAndAppendText(rootEle, "genre", genre);
    }

    for (const auto& country : videoInfo.videoDetail.countries) {
        createAndAppendText(rootEle, "country", country);
    }

    // 根据Kodi的Wiki, 只有电影才有编剧信息, 电视剧才有是否完结和季编号
    if (videoInfo.videoType == MOVIE) {
        for (const auto& credit : videoInfo.videoDetail.credits) {
            createAndAppendText(rootEle, "credit", credit);
        }
    } else if (videoInfo.videoType == TV) {
        const std::string& statusStr = videoInfo.videoDetail.isEnded ? "Ended" : "Continuing";
        createAndAppendText(rootEle, "status", statusStr);
        createAndAppendText(rootEle, "season", std::to_string(videoInfo.videoDetail.seasonNumber));
    }

    createAndAppendText(rootEle, "director", videoInfo.videoDetail.director);
    createAndAppendText(rootEle, "premiered", videoInfo.videoDetail.premiered);

    for (const auto& studio : videoInfo.videoDetail.studio) {
        createAndAppendText(rootEle, "studio", studio);
    }

    for (const auto& actor : videoInfo.videoDetail.actors) {
        AutoPtr<Element> actorEle = dom->createElement("actor");
        createAndAppendText(actorEle, "name", actor.name);
        createAndAppendText(actorEle, "role", actor.role);
        createAndAppendText(actorEle, "order", std::to_string(actor.order));
        createAndAppendText(actorEle, "thumb", actor.thumb);
        rootEle->appendChild(actorEle);
    }

    DOMWriter writer;
    writer.setNewLine("\n");
    writer.setOptions(XMLWriter::PRETTY_PRINT);
    writer.writeNode(ofs, dom);

    return true;
}

template <typename T>
T GetNodeValByPath(Node* parentNode, const std::string& nodePath, T defaultVal = T())
{
    auto childNode = parentNode->getNodeByPath(nodePath);
    T    val       = defaultVal;
    if (childNode != nullptr) {
        std::istringstream iSS(childNode->innerText());
        iSS >> val;
    }

    return val;
};

template <>
std::string GetNodeValByPath(Node* parentNode, const std::string& nodePath, std::string defaultVal)
{
    auto        childNode = parentNode->getNodeByPath(nodePath);
    std::string val       = defaultVal;
    if (childNode != nullptr) {
        std::istringstream iSS(childNode->innerText());
        val = iSS.str();
    }

    return val;
};

bool ParseNfoToVideoInfo(VideoInfo& videoInfo)
{
    // NFO文件的状态必须是格式匹配的
    if (videoInfo.nfoStatus != NFO_FORMAT_MATCH) {
        LOG_ERROR("Nfo format mismatch: {}", videoInfo.nfoPath);
        return false;
    }

    // 获取根节点
    DOMParser         parser;
    AutoPtr<Document> dom         = parser.parse(videoInfo.nfoPath);
    auto              rootElement = dom->documentElement();

    videoInfo.videoDetail.title         = GetNodeValByPath<std::string>(rootElement, "/title");
    videoInfo.videoDetail.originaltitle = GetNodeValByPath<std::string>(rootElement, "/originaltitle");

    // 有些电视剧可能刮削的时候未填写季编号, 则将其填写为1, 即默认为第一部/季, 未填写是否完结的默认完结
    if (videoInfo.videoType == TV) {
        videoInfo.videoDetail.seasonNumber = GetNodeValByPath<int>(rootElement, "/season", 1);
        videoInfo.videoDetail.isEnded =
            GetNodeValByPath<std::string>(rootElement, "/status", "Ended") == "Ended" ? true : false;
    }

    videoInfo.videoDetail.ratings.rating = GetNodeValByPath<double>(rootElement, "/ratings/rating/value");
    videoInfo.videoDetail.ratings.votes  = GetNodeValByPath<int>(rootElement, "/ratings/rating/votes");
    videoInfo.videoDetail.plot           = GetNodeValByPath<std::string>(rootElement, "/plot");
    videoInfo.videoDetail.uniqueid       = GetNodeValByPath<int>(rootElement, "/uniqueid");

    AutoPtr<NodeList> genreNodes = rootElement->getElementsByTagName("genre");
    for (uint32_t i = 0; i < genreNodes->length(); i++) {
        videoInfo.videoDetail.genre.push_back(genreNodes->item(i)->innerText());
    }

    AutoPtr<NodeList> countryNodes = rootElement->getElementsByTagName("country");
    for (uint32_t i = 0; i < countryNodes->length(); i++) {
        videoInfo.videoDetail.countries.push_back(countryNodes->item(i)->innerText());
    }

    videoInfo.videoDetail.director  = GetNodeValByPath<std::string>(rootElement, "/director");
    videoInfo.videoDetail.premiered = GetNodeValByPath<std::string>(rootElement, "/premiered");

    AutoPtr<NodeList> studioNodes = rootElement->getElementsByTagName("studio");
    for (uint32_t i = 0; i < studioNodes->length(); i++) {
        videoInfo.videoDetail.studio.push_back(studioNodes->item(i)->innerText());
    }

    AutoPtr<NodeList> actorNodes = rootElement->getElementsByTagName("actor");
    for (uint32_t i = 0; i < actorNodes->length(); i++) {
        auto        actorNode = actorNodes->item(i);
        ActorDetail actor{
            GetNodeValByPath<std::string>(actorNode, "/name"),
            GetNodeValByPath<std::string>(actorNode, "/role"),
            GetNodeValByPath<int>(actorNode, "order", i),
            GetNodeValByPath<std::string>(actorNode, "/thumb"),
        };
        videoInfo.videoDetail.actors.push_back(actor);
    }

    return true;
}

bool ParseMovieDetailsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail)
{
    Parser      parser;
    auto        result  = parser.parse(sS);
    Object::Ptr jsonPtr = result.extract<Object::Ptr>();

    videoDetail.title          = jsonPtr->getValue<std::string>("title");
    videoDetail.originaltitle  = jsonPtr->getValue<std::string>("original_title");
    videoDetail.ratings.rating = jsonPtr->getValue<double>("vote_average");
    videoDetail.ratings.votes  = jsonPtr->getValue<int>("vote_count");
    videoDetail.plot           = jsonPtr->getValue<std::string>("overview");
    videoDetail.uniqueid       = jsonPtr->getValue<int>("id");

    auto genreJsonPtr = jsonPtr->getArray("genres");
    for (std::size_t i = 0; i < genreJsonPtr->size(); i++) {
        videoDetail.genre.push_back(genreJsonPtr->getObject(i)->getValue<std::string>("name"));
    }

    auto contriesJsonPtr = jsonPtr->getArray("production_countries");
    for (std::size_t i = 0; i < contriesJsonPtr->size(); i++) {
        const std::string& code = contriesJsonPtr->getObject(i)->getValue<std::string>("iso_3166_1");
        videoDetail.countries.push_back(CODE_TO_STR.at(code));
    }

    videoDetail.premiered = jsonPtr->getValue<std::string>("release_date");

    auto studioJsonArrPtr = jsonPtr->getArray("production_companies");
    for (std::size_t i = 0; i < studioJsonArrPtr->size(); i++) {
        videoDetail.studio.push_back(studioJsonArrPtr->getObject(i)->getValue<std::string>("name"));
    }

    return true;
}

bool ParseCreditsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail)
{
    Parser      parser;
    auto        result  = parser.parse(sS);
    Object::Ptr jsonPtr = result.extract<Object::Ptr>();

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

bool WriteEpisodeNfo(const Array::Ptr jsonArrPtr, const std::vector<std::string>& episodePaths)
{
    // TODO: 如果TMDB提供的剧集数量与本地数量不符, 是否使用内置规则生成默认标题
    bool isEpisodeCountMatch = false;
    if (jsonArrPtr->size() == episodePaths.size()) {
        isEpisodeCountMatch = true;
    } else {
        LOG_WARN("TMDB API returns mismatched episode count, using default rules to generate title!");
    }

    for (size_t i = 0; i < episodePaths.size(); i++) {
        const std::string& episodeNfoPath =
            Poco::Path(episodePaths.at(i)).parent().toString() + Poco::Path(episodePaths.at(i)).getBaseName() + ".nfo";
        std::ofstream ofs(episodeNfoPath);
        if (!ofs.is_open()) {
            LOG_ERROR("Can't open file {} for write.", episodeNfoPath);
            return false;
        }

        // TODO: 使用XML方式写入process instruction
        ofs << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)" << std::endl;

        AutoPtr<Document> dom     = new Document();
        AutoPtr<Element>  rootEle = dom->createElement("episodedetails");
        dom->appendChild(rootEle);

        auto createAndAppendText = [dom](Element* parent, const std::string& tagName, const std::string& text) {
            AutoPtr<Element> ele      = dom->createElement(tagName);
            AutoPtr<Text>    textNode = dom->createTextNode(text);
            ele->appendChild(textNode);
            parent->appendChild(ele);
        };

        auto        episodeJsonPtr = jsonArrPtr->getObject(i);
        std::string title =
            isEpisodeCountMatch ? episodeJsonPtr->getValue<std::string>("name") : "第" + std::to_string(i + 1) + "集";
        createAndAppendText(rootEle, "title", title);

        createAndAppendText(rootEle, "season", std::to_string(episodeJsonPtr->getValue<int>("season_number")));
        createAndAppendText(rootEle, "episode", std::to_string(episodeJsonPtr->getValue<int>("episode_number")));
        createAndAppendText(rootEle, "plot", episodeJsonPtr->getValue<std::string>("overview"));
        // TODO: 添加更多详细标签

        DOMWriter writer;
        writer.setNewLine("\n");
        writer.setOptions(XMLWriter::PRETTY_PRINT);
        writer.writeNode(ofs, dom);
    }

    return true;
}

bool SetTVEnded(const std::string& nfoPath)
{
    std::ofstream ofs(nfoPath);
    if (!ofs.is_open()) {
        LOG_ERROR("Can't open file {} for write.", nfoPath);
        return false;
    }

    // 获取根节点
    auto             dom         = DOMParser().parse(nfoPath);
    AutoPtr<Element> rootElement = dom->documentElement();

    auto statusNode = rootElement->getNodeByPath("/status");
    statusNode->setNodeValue("Ended");

    DOMWriter writer;
    writer.setNewLine("\n");
    writer.setOptions(XMLWriter::PRETTY_PRINT);
    writer.writeNode(ofs, dom);

    return true;
}
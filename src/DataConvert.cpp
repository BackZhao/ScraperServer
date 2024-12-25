#include "DataConvert.h"

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

#include "HDRToolKit.h"
#include "Logger.h"

using Poco::AutoPtr;
using namespace Poco::XML;
using namespace Poco::JSON;

void VideoInfoToBriefJson(const VideoInfo& videoInfo, Object& outJson)
{
    outJson.set("VideoType", VIDEO_TYPE_TO_STR.at(videoInfo.videoType));
    outJson.set("VideoPath", videoInfo.videoPath);
    outJson.set("NfoStatus", static_cast<int>(videoInfo.nfoStatus));
    outJson.set("PosterStatus", static_cast<int>(videoInfo.posterStatus));
    outJson.set("HDRType", VIDEO_RANGE_TYPE_TO_STR_MAP.at(videoInfo.hdrType));
}

void VideoInfoToDetailedJson(const VideoInfo& videoInfo, Object& outJson)
{
    VideoInfoToBriefJson(videoInfo, outJson);
    outJson.set("VideoPath", videoInfo.videoPath);
    outJson.set("NfoPath", videoInfo.nfoPath);
    outJson.set("PosterPath", videoInfo.posterPath);

    Object videoDetailJson;
    if (videoInfo.videoType == TV) {
        videoDetailJson.set("EpisodeNfoCount", videoInfo.videoDetail.episodeNfoCount);
        videoDetailJson.set("EpisodeCount", videoInfo.videoDetail.episodePaths.size());
        Array episodePathsArrJson;
        for (const auto& episodePath : videoInfo.videoDetail.episodePaths) {
            episodePathsArrJson.add(episodePath);
        }
        videoDetailJson.set("EpisodePaths", episodePathsArrJson);
    }

    // 如果NFO文件格式匹配, 则额外填写NFO的信息
    if (videoInfo.nfoStatus == FILE_FORMAT_MATCH) {
        videoDetailJson.set("Title", videoInfo.videoDetail.title);
        videoDetailJson.set("OriginalTitle", videoInfo.videoDetail.originaltitle);

        Object ratingsJson;
        ratingsJson.set("Rating", videoInfo.videoDetail.ratings.rating);
        ratingsJson.set("Votes", videoInfo.videoDetail.ratings.votes);
        videoDetailJson.set("Ratings", ratingsJson);

        videoDetailJson.set("Plot", videoInfo.videoDetail.plot);
        videoDetailJson.set("Uniqueid", videoInfo.videoDetail.uniqueid.at("tmdb"));

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
        }
    }
    outJson.set("VideoDetail", videoDetailJson);
}

bool VideoInfoToNfo(const VideoInfo&   videoInfo,
                    const std::string& nfoPath,
                    bool               setHDRTitle,
                    const std::string& defaultIdType)
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

    if (setHDRTitle && videoInfo.hdrType != VideoRangeType::SDR && videoInfo.hdrType != VideoRangeType::UNKNOWN) {
        std::string title = videoInfo.videoDetail.title;
        std::string hdrTitle = VIDEO_RANGE_TYPE_TO_STR_MAP.at(videoInfo.hdrType);
        if (title.find(hdrTitle) == std::string::npos) {
            createAndAppendText(rootEle, "title", title + " " + hdrTitle);
        } else {
            createAndAppendText(rootEle, "title", title);
        }
    } else {
        createAndAppendText(rootEle, "title", videoInfo.videoDetail.title);
    }
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

    for (auto pair : videoInfo.videoDetail.uniqueid) {
        AutoPtr<Element> uniqueidEle = dom->createElement("uniqueid");
        uniqueidEle->setAttribute("type", pair.first);
        if (pair.first == defaultIdType) {
            uniqueidEle->setAttribute("default", "true");
        }
        AutoPtr<Text> uniqueidText = dom->createTextNode(std::to_string(pair.second));
        uniqueidEle->appendChild(uniqueidText);
        rootEle->appendChild(uniqueidEle);
    }

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
    if (videoInfo.nfoStatus != FILE_FORMAT_MATCH) {
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

    AutoPtr<NodeList> IdNodes = rootElement->getElementsByTagName("uniqueid");
    for (uint32_t i = 0; i < IdNodes->length(); i++) {
        Element* ele = dynamic_cast<Element *>(IdNodes->item(i));
        std::string idType        = ele->getAttribute("type");
        // FIXME: IMDB的ID包含字母"tt", 暂时只解析"themoviedb"的ID
        if (idType != "tmdb") {
            continue;
        }
        videoInfo.videoDetail.uniqueid[idType] = std::stoi(ele->innerText());
    }

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

bool WriteEpisodeNfo(const Array::Ptr                jsonArrPtr,
                     const std::vector<std::string>& episodePaths,
                     int                             seasonId,
                     bool                            forceUseOnlineTvMeta)
{
    // TODO: 如果TMDB提供的剧集数量与本地数量不符, 是否使用内置规则生成默认标题
    bool isEpisodeCountMatch = false;
    if (jsonArrPtr->size() == episodePaths.size()) {
        isEpisodeCountMatch = true;
    } else {
        LOG_WARN(
            "TMDB API returns mismatched episode count! api: {}, local: {}", jsonArrPtr->size(), episodePaths.size());
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

        auto episodeJsonPtr = jsonArrPtr->getObject(i);
        if (isEpisodeCountMatch) {
            std::string title = episodeJsonPtr->getValue<std::string>("name");
            createAndAppendText(rootEle, "title", title);
            createAndAppendText(rootEle, "season", std::to_string(episodeJsonPtr->getValue<int>("season_number")));
            createAndAppendText(rootEle, "episode", std::to_string(episodeJsonPtr->getValue<int>("episode_number")));
            createAndAppendText(rootEle, "plot", episodeJsonPtr->getValue<std::string>("overview"));
        } else {
            // 强制使用在线剧集元数据时, 大于在线集数的使用生成的元数据
            if (forceUseOnlineTvMeta && i < jsonArrPtr->size()) {
                std::string title = episodeJsonPtr->getValue<std::string>("name");
                createAndAppendText(rootEle, "title", title);
                createAndAppendText(rootEle, "season", std::to_string(episodeJsonPtr->getValue<int>("season_number")));
                createAndAppendText(
                    rootEle, "episode", std::to_string(episodeJsonPtr->getValue<int>("episode_number")));
                createAndAppendText(rootEle, "plot", episodeJsonPtr->getValue<std::string>("overview"));
            } else {
                createAndAppendText(rootEle, "title", "第" + std::to_string(i + 1) + "集");
                createAndAppendText(rootEle, "season", std::to_string(seasonId));
                createAndAppendText(rootEle, "episode", std::to_string(i + 1));
                createAndAppendText(rootEle, "plot", "");
            }
        }

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
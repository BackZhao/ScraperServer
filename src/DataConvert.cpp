#include "DataConvert.h"

#include "Logger.h"

#include <fstream>

#include <Poco/AutoPtr.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/DOMWriter.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/NodeList.h>
#include <Poco/StreamCopier.h>
#include <Poco/XML/XMLWriter.h>
#include <Poco/DOM/Text.h>
#include <Poco/Path.h>

using Poco::AutoPtr;
using namespace Poco::XML;

void VideoInfoToBriefJson(uint32_t id, const VideoInfo& videoInfo, Poco::JSON::Object& outJson)
{
    outJson.set("Id", id);
    outJson.set("VideoType", VIDEO_TYPE_TO_STR.at(videoInfo.videoType));
    outJson.set("VideoPath", videoInfo.videoPath);
    outJson.set("NfoStatus", static_cast<int>(videoInfo.nfoStatus));
    outJson.set("PosterStatus", static_cast<int>(videoInfo.posterStatus));
    outJson.set("NfoPath", videoInfo.nfoPath);
    outJson.set("PosterPath", videoInfo.posterPath);
}

void VideoInfoToDetailedJson(uint32_t id, const VideoInfo& videoInfo, Poco::JSON::Object& outJson)
{
    VideoInfoToBriefJson(id, videoInfo, outJson);

    // 如果NFO文件格式匹配, 则额外填写NFO的信息
    if (videoInfo.nfoStatus == NFO_FORMAT_MATCH) {
        Poco::JSON::Object videoDetailJson;
        videoDetailJson.set("Title", videoInfo.videoDetail.title);
        videoDetailJson.set("OriginalTitle", videoInfo.videoDetail.originaltitle);

        Poco::JSON::Object userRatingJson;
        userRatingJson.set("Rating", videoInfo.videoDetail.userrating.rating);
        userRatingJson.set("Votes", videoInfo.videoDetail.userrating.votes);
        videoDetailJson.set("UserRating", userRatingJson);

        videoDetailJson.set("Plot", videoInfo.videoDetail.plot);
        videoDetailJson.set("Uniqueid", videoInfo.videoDetail.uniqueid);

        Poco::JSON::Array genreArrJson;
        for (const auto& genre : videoInfo.videoDetail.genre) {
            genreArrJson.add(genre);
        }
        videoDetailJson.set("Genre", genreArrJson);

        Poco::JSON::Array contriesArrJson;
        for (const auto& country : videoInfo.videoDetail.countries) {
            contriesArrJson.add(country);
        }
        videoDetailJson.set("Countries", contriesArrJson);

        Poco::JSON::Array creditsArrJson;
        for (const auto& credit : videoInfo.videoDetail.credits) {
            creditsArrJson.add(credit);
        }
        videoDetailJson.set("Credits", creditsArrJson);

        videoDetailJson.set("Director", videoInfo.videoDetail.director);
        videoDetailJson.set("Premiered", videoInfo.videoDetail.premiered);
        videoDetailJson.set("Studio", videoInfo.videoDetail.studio);

        Poco::JSON::Array actorsArrJson;
        for (const auto& actor : videoInfo.videoDetail.actors) {
            Poco::JSON::Object actorJson;
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
            Poco::JSON::Array episodePathsArrJson;
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

    AutoPtr<Document> dom = new Document();
    AutoPtr<Element> rootEle = dom->createElement(rootTagStr.at(videoInfo.videoType));
    dom->appendChild(rootEle);

    auto createAndAppendText = [dom](Element* parent, const std::string& tagName, const std::string& text) {
        AutoPtr<Element> ele = dom->createElement(tagName);
        AutoPtr<Text> textNode = dom->createTextNode(text);
        ele->appendChild(textNode);
        parent->appendChild(ele);
    };

    createAndAppendText(rootEle, "title", videoInfo.videoDetail.title);
    createAndAppendText(rootEle, "season", std::to_string(videoInfo.videoDetail.seasonNumber));
    createAndAppendText(rootEle, "originaltitle", videoInfo.videoDetail.originaltitle);

    AutoPtr<Element> ratingsEle = dom->createElement("ratings");
    rootEle->appendChild(ratingsEle);
    AutoPtr<Element> ratingEle = dom->createElement("rating");
    ratingEle->setAttribute("max", "10");
    ratingEle->setAttribute("default", "true");
    ratingEle->setAttribute("name", "themoviedb");
    ratingsEle->appendChild(ratingEle);
    createAndAppendText(ratingEle, "values", std::to_string(videoInfo.videoDetail.userrating.rating));
    createAndAppendText(ratingEle, "votes", std::to_string(videoInfo.videoDetail.userrating.votes));

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

    // 根据Kodi的Wiki, 只有电影才有编剧信息, 电视剧才有是否完结
    if (videoInfo.videoType == MOVIE) {
        for (const auto& credit : videoInfo.videoDetail.credits) {
            createAndAppendText(rootEle, "credit", credit);
        }
    } else if (videoInfo.videoType == TV) {
        const std::string& statusStr = videoInfo.videoDetail.isEnded ? "Ended" : "Continuing";
        createAndAppendText(rootEle, "status", statusStr);
    }

    createAndAppendText(rootEle, "director", videoInfo.videoDetail.director);
    createAndAppendText(rootEle, "premiered", videoInfo.videoDetail.premiered);
    createAndAppendText(rootEle, "studio", videoInfo.videoDetail.studio);

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

bool ParseNfoToVideoInfo(VideoInfo& videoInfo)
{
    // NFO文件的状态必须是格式匹配的
    if (videoInfo.nfoStatus != NFO_FORMAT_MATCH) {
        LOG_ERROR("Nfo format mismatch: {}", videoInfo.nfoPath);
        return false;
    }

    // 获取根节点
    auto dom         = DOMParser().parse(videoInfo.nfoPath);
    AutoPtr<Element> rootElement = dom->documentElement();

    videoInfo.videoDetail.title         = rootElement->getNodeByPath("/title")->innerText();
    videoInfo.videoDetail.originaltitle = rootElement->getNodeByPath("/originaltitle")->innerText();

    // 有些电视剧可能刮削的时候未填写季编号, 则将其填写为1, 即默认为第一部/季, 未填写是否完结的默认完结
    if (videoInfo.videoType == TV) {
        auto seasonNumNode = rootElement->getNodeByPath("/season");
        videoInfo.videoDetail.seasonNumber = seasonNumNode == nullptr ? 1 : std::stoi(seasonNumNode->innerText());

        auto statusNode = rootElement->getNodeByPath("/status");
        videoInfo.videoDetail.isEnded = ((statusNode == nullptr) ? true : (statusNode->innerText() == "Ended")); 
    }

    videoInfo.videoDetail.userrating.rating =
        std::stod(rootElement->getNodeByPath("/ratings/rating/values")->innerText());
    videoInfo.videoDetail.userrating.votes =
        std::stoi(rootElement->getNodeByPath("/ratings/rating/votes")->innerText());
    videoInfo.videoDetail.plot     = rootElement->getNodeByPath("/plot")->innerText();
    videoInfo.videoDetail.uniqueid = std::stoi(rootElement->getNodeByPath("/uniqueid")->innerText());

    auto genreNodes = rootElement->getElementsByTagName("genre");
    for (uint32_t i = 0; i < genreNodes->length(); i++) {
        videoInfo.videoDetail.genre.push_back(genreNodes->item(i)->innerText());
    }

    auto countryNodes = rootElement->getElementsByTagName("country");
    for (uint32_t i = 0; i < countryNodes->length(); i++) {
        videoInfo.videoDetail.countries.push_back(countryNodes->item(i)->innerText());
    }

    videoInfo.videoDetail.director  = rootElement->getNodeByPath("/director")->innerText();
    videoInfo.videoDetail.premiered = rootElement->getNodeByPath("/premiered")->innerText();
    videoInfo.videoDetail.studio    = rootElement->getNodeByPath("/studio")->innerText();

    auto actorNodes = rootElement->getElementsByTagName("actor");
    for (uint32_t i = 0; i < actorNodes->length(); i++) {
        ActorDetail actor{
            actorNodes->item(i)->getNodeByPath("/name")->innerText(),
            actorNodes->item(i)->getNodeByPath("/role")->innerText(),
            std::stoi(actorNodes->item(i)->getNodeByPath("/order")->innerText()),
            actorNodes->item(i)->getNodeByPath("/thumb")->innerText(),
        };
        videoInfo.videoDetail.actors.push_back(actor);
    }

    return true;
}

bool WriteEpisodeNfo(const Poco::JSON::Array::Ptr jsonArrPtr, const std::vector<std::string>& episodePaths)
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

        auto episodeJsonPtr = jsonArrPtr->getObject(i);
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
#include "Config.h"

#include <fstream>

#include <Poco/File.h>
#include <Poco/FileStream.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Path.h>
#include <Poco/StreamCopier.h>

#include "Logger.h"
#include "Utils.h"

const std::string CONF_FILE_NAME           = "ScraperServer.json"; // 默认的配置文件名称

Config& Config::Instance()
{
    static Config singleton;
    return singleton;
}

void Config::GetConfFile()
{
    static const std::vector<std::string> confPaths = {
        Poco::Path::configHome() + "ScraperServer" + Poco::Path::separator() + "config" + Poco::Path::separator() +
            CONF_FILE_NAME,
        Poco::Path::current() + "config" + Poco::Path::separator() + CONF_FILE_NAME,
        Poco::Path::current() + CONF_FILE_NAME,
    };

    for (const auto& confPath : confPaths) {
        Poco::File file(confPath);
        if (file.exists()) {
            m_confFile = confPath;
            return;
        }
    }
}

void Config::SetPort(uint16_t portNum)
{
    m_appConf.networkConf.listenPort = portNum;
}

void Config::SetConfFile(const std::string& confFile)
{
    m_confFile = confFile;
}

void Config::SetLogLevel(int logLevel)
{
    m_appConf.logLevel = logLevel;
}

void Config::SetAuto(bool isAuto)
{
    m_appConf.isAuto = isAuto;
}

void Config::DecreaseLogLevel()
{
    if (m_appConf.logLevel > 0) {
        m_appConf.logLevel--;
    }
}

void Config::SetLogFile(const std::string& logLevel)
{
    m_appConf.logFile = logLevel;
}

uint16_t Config::GetPort()
{
    return m_appConf.networkConf.listenPort;
}

std::string Config::GetHttpProxyHost()
{
    return m_appConf.networkConf.httpProxyHost;
}

uint16_t Config::GetHttpProxyPort()
{
    return m_appConf.networkConf.httpProxyPort;
}

int Config::GetLogLevel()
{
    // 未设置HTTP服务器的监听端口时, 返回默认值
    return m_appConf.logLevel;
}

std::string Config::GetLogFile()
{
    // 未设置HTTP服务器的监听端口时, 返回默认值
    return m_appConf.logFile;
}

int Config::GetAutoInterval()
{
    return m_appConf.autoInterval;
}

std::string Config::GetImageDownloadQuality()
{
    return m_appConf.apiConf.imageDownloadQuality;
}

bool Config::IsAuto()
{
    return m_appConf.isAuto;
}

const std::map<VideoType, std::vector<std::string>>& Config::GetPaths()
{
    return m_appConf.dataSourceConf.paths;
}

const std::string& Config::GetApiUrl(ApiUrlType apiUrlType)
{
    return m_appConf.apiConf.apiUrls.at(apiUrlType);
}

const std::string& Config::GetApiKey()
{
    return m_appConf.apiConf.apiKey;
}

bool Config::ParseConfFile()
{
    // 如果命令行参数解析时没有设置配置文件, 则按照预设规则搜索配置文件
    if (m_confFile.empty()) {
        GetConfFile();
    }

    // 未找到配置文件时, 设置HTTP服务器的端口号为默认值8000
    if (m_confFile.empty()) {
        LOG_ERROR("No conf file named {} found!", CONF_FILE_NAME);
        return true;
    }

    LOG_INFO("Using conf file: {}", m_confFile);

    try {
        Poco::FileInputStream fis(m_confFile);
        std::ostringstream    ostr;
        Poco::StreamCopier::copyStream(fis, ostr);

        std::istringstream      istr(ostr.str());
        Poco::JSON::Parser      jsonParser;
        auto                    result  = jsonParser.parse(istr);
        Poco::JSON::Object::Ptr jsonPtr = result.extract<Poco::JSON::Object::Ptr>();

        m_appConf.logLevel = ParseLogLevel(jsonPtr->optValue<std::string>("LogLevel", "info"));
        m_appConf.autoInterval = jsonPtr->optValue<int>("AutoInterval", AUTO_INTERVAL);

        auto apiConfJson         = jsonPtr->getObject("API");
        m_appConf.apiConf.apiKey = apiConfJson->getValue<std::string>("APIKey");

        auto networkConfJson = jsonPtr->getObject("Network");
        if (m_appConf.networkConf.listenPort != 0) {
            LOG_WARN("HTTP listen port is specified by cli, ignore value in config file.");
        } else {
            m_appConf.networkConf.listenPort =
                networkConfJson->optValue<uint16_t>("ListenPort", DEFAULT_HTTP_SERVER_PORT);
        }
        auto httpProxyJson = networkConfJson->getObject("HttpProxy");
        m_appConf.networkConf.httpProxyHost = httpProxyJson->getValue<std::string>("Host");
        m_appConf.networkConf.httpProxyPort = httpProxyJson->getValue<uint16_t>("Port");

        auto apiUrlsJson                             = apiConfJson->getObject("URLs");
        m_appConf.apiConf.apiUrls[SEARCH_MOVIE]      = apiUrlsJson->getValue<std::string>("SearchMovie");
        m_appConf.apiConf.apiUrls[SEARCH_TV]         = apiUrlsJson->getValue<std::string>("SearchTV");
        m_appConf.apiConf.apiUrls[GET_MOVIE_CREDITS] = apiUrlsJson->getValue<std::string>("GetMovieCredits");
        m_appConf.apiConf.apiUrls[GET_MOVIE_DETAIL]  = apiUrlsJson->getValue<std::string>("GetMovieDetail");
        m_appConf.apiConf.apiUrls[GET_MOVIE_IMAGES]  = apiUrlsJson->getValue<std::string>("GetMovieImages");
        m_appConf.apiConf.apiUrls[GET_TV_CREDITS]    = apiUrlsJson->getValue<std::string>("GetTVCredits");
        m_appConf.apiConf.apiUrls[GET_TV_DETAIL]     = apiUrlsJson->getValue<std::string>("GetTVDetail");
        m_appConf.apiConf.apiUrls[GET_TV_IMAGES]     = apiUrlsJson->getValue<std::string>("GetTVImages");
        m_appConf.apiConf.apiUrls[GET_SEASON_DETAIL] = apiUrlsJson->getValue<std::string>("GetSeasonDetail");
        m_appConf.apiConf.apiUrls[GET_SEASON_IMAGES] = apiUrlsJson->getValue<std::string>("GetSeasonImages");
        m_appConf.apiConf.apiUrls[IMAGE_DOWNLOAD]    = apiUrlsJson->getValue<std::string>("ImageDownload");

        auto apiTimeoutJson               = apiConfJson->getObject("Timeout");
        m_appConf.apiConf.downloadTimeout = apiTimeoutJson->getValue<int>("Download");
        m_appConf.apiConf.jsonTimeout     = apiTimeoutJson->getValue<int>("Json");

        auto apiQualityJson                    = apiConfJson->getObject("Quality");
        m_appConf.apiConf.imageDownloadQuality = apiQualityJson->getValue<std::string>("ImageDownload");
        m_appConf.apiConf.imagePreviewQuality  = apiQualityJson->getValue<std::string>("ImagePreview");

        auto dataSourceJson                      = jsonPtr->getObject("DataSource");
        m_appConf.dataSourceConf.refreshInterval = dataSourceJson->getValue<int>("RefreshInterval");
        for (const auto& pathVar : *(dataSourceJson->getArray("Movies"))) {
            m_appConf.dataSourceConf.paths[MOVIE].push_back(pathVar.toString());
        }
        for (const auto& pathVar : *(dataSourceJson->getArray("TVs"))) {
            m_appConf.dataSourceConf.paths[TV].push_back(pathVar.toString());
        }
    } catch (Poco::Exception& e) {
        LOG_ERROR("Parse conf file {} failed: {}", m_confFile, e.displayText());
        return false;
    }

    return true;
}

void Config::SaveToFile()
{
    // 加锁
    std::lock_guard<std::mutex> locker(m_writeLock);

    Poco::JSON::Object jsonObj;
    jsonObj.set("port", m_appConf.networkConf.listenPort);

    std::ofstream ofs(m_confFile);
    try {
        jsonObj.stringify(ofs, 2);
    } catch (Poco::Exception& e) {
        std::cerr << "Config file " << m_confFile << " failed: " << e.displayText() << std::endl;
    }
}

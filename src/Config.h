#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <cstdint>

#include "CommonType.h"

const int DEFAULT_HTTP_SERVER_PORT = 54250; // 默认的HTTP服务器监听端口
const int AUTO_INTERVAL            = 300;   // 默认的自动刮削间隔, 单位: 秒

/**
 * @brief 与网络相关的配置项
 *
 */
struct NetworkConf {
    uint16_t    listenPort = DEFAULT_HTTP_SERVER_PORT; // HTTP服务器监听的端口号
    std::string httpProxyHost;                         // HTTP代理的主机名
    uint16_t    httpProxyPort;                         // HTTP代理的端口号
};

/**
 * @brief API的URL类型
 *
 */
enum ApiUrlType {
    SEARCH_MOVIE,
    SEARCH_TV,
    GET_MOVIE_CREDITS,
    GET_MOVIE_DETAIL,
    GET_MOVIE_IMAGES,
    GET_TV_CREDITS,
    GET_TV_DETAIL,
    GET_TV_IMAGES,
    GET_SEASON_DETAIL,
    GET_SEASON_IMAGES,
    IMAGE_DOWNLOAD,
};

/**
 * @brief API相关的配置项
 *
 */
struct ApiConf {
    std::string                       apiKey;               // API秘钥
    std::map<ApiUrlType, std::string> apiUrls;              // API的URL
    int                               downloadTimeout;      // 图像下载的超时时间
    int                               jsonTimeout;          // 获取JSON的超时时间
    std::string                       imageDownloadQuality; // 图像下载的质量
    std::string                       imagePreviewQuality;  // 图像预览的质量
};

/**
 * @brief 数据源配置信息
 *
 */
struct DataSourceConf {
    int                                           refreshInterval; // 定时刷新刮削数据源的间隔
    std::map<VideoType, std::vector<std::string>> paths;
};

/**
 * @brief 程序的配置项
 *
 */
struct AppConf {
    NetworkConf    networkConf;
    int            logLevel;
    std::string    logFile;
    ApiConf        apiConf;
    DataSourceConf dataSourceConf;
    int            autoInterval = AUTO_INTERVAL; // 自动刮削的间隔
    bool           isAuto;                       // 是否为自动刮削模式
};

class Config
{
public:

    /**
     * @brief 获取单例
     *
     * @return Config& 单例
     */
    static Config& Instance();

    /**
     * @brief 解析配置文件
     *
     * @return true
     * @return false
     */
    bool ParseConfFile();

    /**
     * @brief 设置HTTP服务器监听的端口号
     *
     * @param portNum HTTP服务器监听的端口号
     */
    void SetPort(uint16_t portNum);

    /**
     * @brief 设置应用的配置文件路径
     *
     * @param confFile 应用的配置文件路径
     */
    void SetConfFile(const std::string& confFile);

    /**
     * @brief 设置日志等级
     *
     * @param logLevel 日志等级
     */
    void SetLogLevel(int logLevel);

    /**
     * @brief 设置是否为自动刮削模式
     *
     * @param isAuto 是否开启自动刮削
     */
    void SetAuto(bool isAuto);

    /**
     * @brief 降低日志等级
     *
     */
    void DecreaseLogLevel();

    /**
     * @brief 设置日志文件路径
     *
     * @param logFile 日志文件路径
     */
    void SetLogFile(const std::string& logLevel);

    /**
     * @brief 获取配置中的端口号
     *
     * @return uint16_t 端口号
     */
    uint16_t GetPort();

    /**
     * @brief 获取配置中的HTTP代理主机
     *
     * @return std::string HTTP代理主机
     */
    std::string GetHttpProxyHost();

    /**
     * @brief 获取配置中的HTTP代理端口号
     *
     * @return uint16_t HTTP代理端口号
     */
    uint16_t GetHttpProxyPort();

    /**
     * @brief 获取日志等级
     *
     * @return int 日志等级
     */
    int GetLogLevel();

    /**
     * @brief 获取日志文件路径
     *
     * @return std::string 日志文件路径
     */
    std::string GetLogFile();

    /**
     * @brief 获取自动刮削的时间间隔
     *
     * @return int 自动刮削的时间间隔
     */
    int GetAutoInterval();

    /**
     * @brief 获取图像下载的质量
     *
     * @return std::string 图像下载质量
     */
    std::string GetImageDownloadQuality();

    /**
     * @brief 是否开启了自动刮削
     *
     * @return true 是
     * @return false 否
     */
    bool IsAuto();

    const std::map<VideoType, std::vector<std::string>>& GetPaths();

    const std::string& GetApiUrl(ApiUrlType apiUrlType);
    const std::string& GetApiKey();

    /**
     * @brief 保存配置到配置文件
     *
     */
    void SaveToFile();

private:

    /**
     * @brief 单例模式, 构造函数私有化
     *
     */
    Config()
    {
        m_appConf.isAuto                  = false;
        m_appConf.networkConf.listenPort  = 0;
        m_appConf.logLevel                = 2; // spdlog::level::info
        m_appConf.apiConf.downloadTimeout = 15;
        m_appConf.apiConf.jsonTimeout     = 5;
    };

    /**
     * @brief 获取配置文件的路径
     *
     */
    void GetConfFile();

private:

    AppConf     m_appConf;   // 程序的配置
    std::string m_confFile;  // 配置文件的路径
    std::mutex  m_writeLock; // 保存配置文件的写入锁
};

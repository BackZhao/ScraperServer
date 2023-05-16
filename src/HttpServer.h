#pragma once

#include "Config.h"

#include "ApiManager.h"

#include <mutex>
#include <thread>

#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>


/**
 * @brief
 *
 */
class HTTPServerApp
{
public:

    /**
     * @brief 无参构造函数
     *
     */
    HTTPServerApp() {}

    /**
     * @brief 析构函数
     *
     */
    ~HTTPServerApp() {
        if (m_httpServer != nullptr) {
            delete m_httpServer;
            m_httpServer = nullptr;
        }
    }

    /**
     * @brief 启动应用
     *
     * @param args
     * @return int
     */
    int run();

private:

    /**
     * @brief 信号处理函数
     * 
     * @param signum 信号的编号
     */
    static void SignalHandler(int signum);

    /**
     * @brief 初始化
     *
     * @param app
     */
    void initialize();

    /**
     * @brief 自动更新NFO文件, 目前仅支持新增的剧集
     *
     */
    void AutoUpdate();

    /**
     * @brief 等待捕获信号
     *
     */
    void WaitingSignal();

private:

    static int             m_signum;                // 程序捕获到的信号
    Poco::Net::HTTPServer* m_httpServer;            // HTTP服务器
    std::thread            m_managerRefreshThread;  // Manager类刷新的线程
    std::thread            m_signalHandleThread;  // Manager类刷新的线程
};

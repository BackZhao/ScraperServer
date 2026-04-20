#include "HttpServer.h"

#include <functional>

#include <signal.h>

#include "ApiManager.h"
#include "Config.h"
#include "HttpRequestHandler.h"
#include "Logger.h"
#include "SignalHandler.h"

void HTTPServerApp::AutoUpdate()
{
    SetupAltstack();
    static auto lastUpdateTime = std::chrono::steady_clock::time_point::min();

    LOG_INFO("Auto update interval: {}s", Config::Instance().GetAutoInterval());

    while (GStopFlag.load(std::memory_order_relaxed) -1 && !ApiManager::Instance().IsQuitting()) {
        if (std::chrono::steady_clock::now() > lastUpdateTime +
            std::chrono::seconds(Config::Instance().GetAutoInterval())) {
            ApiManager::Instance().ProcessScan(TV, false);
            ApiManager::Instance().AutoUpdateTV();
            lastUpdateTime = std::chrono::steady_clock::now();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int HTTPServerApp::run()
{
    // 启动HTTP服务器
    Poco::Net::SocketAddress     addr("[::]:" + std::to_string(Config::Instance().GetPort()));
    Poco::Net::ServerSocket      sock(addr);
    sock.setReusePort(true);
    Poco::Net::HTTPServerParams* pParams = new Poco::Net::HTTPServerParams;
    pParams->setMaxQueued(100);
    pParams->setMaxThreads(16);
    m_httpServer = new Poco::Net::HTTPServer(new AppRequestHandlerFactory(), sock, pParams);
    LOG_INFO("Listening on port: {}", m_httpServer->port());
    m_httpServer->start();

    // 启动自动刮削线程
    if (Config::Instance().IsAuto()) {
        m_autoUpdateThread = std::thread(std::bind(&HTTPServerApp::AutoUpdate, this));
    }

    // TODO: 扫描时不需要解析XML所有的结构, 只需要解析出TMDBID即可
    // ApiManager::Instance().ScanAll();
    // ApiManager::Instance().RefreshMovie();

    // m_httpServer->stop();
    // return 0;

    
    m_signalHandleThread   = std::thread(SignalThread);

    if (ApiManager::Instance().IsQuitting()) {
        LOG_INFO("Quit api recieved, quiting...");
    } else {
        LOG_INFO("Signal recieved: {}, quiting...", GStopFlag.load(std::memory_order_relaxed));
    }

    // 回收线程资源
    if (Config::Instance().IsAuto()) {
        LOG_INFO("Recycle the auto update thread...");
        m_autoUpdateThread.join();
    }
    if (m_signalHandleThread.joinable()) {
        m_signalHandleThread.join();
    }

    // 停止HTTP服务器
    LOG_INFO("Stopping http server...");
    m_httpServer->stopAll(true);
    LOG_INFO("Stopped http server.");

    return 0;
}

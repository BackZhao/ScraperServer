#include "HttpServer.h"

#include "Config.h"
#include "HttpRequestHandler.h"
#include "Logger.h"
#include "ApiManager.h"

#include <signal.h>

#include <functional>

int HTTPServerApp::m_signum = -1;

void HTTPServerApp::AutoUpdate()
{
    static auto lastUpdateTime = std::chrono::steady_clock::time_point::min();

    LOG_INFO("Auto update interval: {}s", Config::Instance().GetAutoInterval());

    while (m_signum == -1) {
        if (std::chrono::steady_clock::now() > lastUpdateTime +
            std::chrono::seconds(Config::Instance().GetAutoInterval())) {
            Poco::JSON::Object jsonObj;
            jsonObj.set("videoType", "tv");
            ApiManager::Instance().Scan(jsonObj, std::cout);
            ApiManager::Instance().AutoUpdateTV();
            lastUpdateTime = std::chrono::steady_clock::now();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void HTTPServerApp::SignalHandler(int signum)
{
    LOG_INFO("Signal recieved: {}", m_signum);
    DataSource::Cancel();
    m_signum = signum;
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
    m_httpServer = new Poco::Net::HTTPServer(new SupervisorRequestHandlerFactory(), sock, pParams);
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

    // 捕获SIGINT和SIGTERM
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    // 等待捕获信号
    while (m_signum == -1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 回收线程资源
    if (Config::Instance().IsAuto()) {
        m_autoUpdateThread.join();
    }

    // 停止HTTP服务器
    m_httpServer->stop();

    return 0;
}

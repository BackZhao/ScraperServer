#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // WIN32

#include <version.h>

#include "ApiManager.h"
#include "Config.h"
#include "DataSource.h"
#include "HttpServer.h"
#include "Logger.h"
#include "Option.h"
#include "ResourceManager.h"
#include "Tmdb.h"

int main(int argc, char* argv[])
{
    // 解析命令行参数
    Option option(argc, argv);
    int    ret = option.Process();
    if (ret <= 0) {
        return ret;
    }

    // 初始化日志记录器
    if (!Logger::Instance().Init(static_cast<spdlog::level::level_enum>(Config::Instance().GetLogLevel()),
                                 Config::Instance().GetLogFile())) {
        return 1;
    }

    if (!ResourceManager::Instance().Init()) {
        LOG_ERROR("Resource init failed!");
        return 1;
    }

    LOG_INFO("version: {}", VERSION);

    // 配置文件解析失败则退出
    if (!Config::Instance().ParseConfFile()) {
        return 1;
    }

    ApiManager::Instance().SetScanPaths(Config::Instance().GetPaths());

    // 是否需要后台运行
    if (option.IsDaemon()) {
#ifdef _WIN32
        HWND hWnd = ::FindWindowA("ConsoleWindowClass", NULL);
        if (hWnd != NULL) {
            ::ShowWindow(hWnd, SW_HIDE);
        }
#else
        if (daemon(1, 0) != 0) {
            perror("daemon");
            return 1;
        }
#endif // WIN32
    }

    HTTPServerApp app;
    return app.run();
}
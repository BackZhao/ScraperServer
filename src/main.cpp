#include "ApiManager.h"

#include "Config.h"
#include "DataSource.h"
#include "HttpServer.h"
#include "Logger.h"
#include "Option.h"
#include "Tmdb.h"
#include "version.h"

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

    LOG_INFO("version: {}", VERSION);

    // 配置文件解析失败则退出
    if (!Config::Instance().ParseConfFile()) {
        return 1;
    }

    ApiManager::Instance().SetScanPaths(Config::Instance().GetPaths());

    HTTPServerApp app;
    return app.run();
}
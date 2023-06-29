#include <iostream>

#include "DataSource.h"
#include "Logger.h"
#include "Config.h"
#include "Option.h"
#include "ApiManager.h"
#include "HttpServer.h"

#include "DataConvert.h"

#include "Tmdb.h"

#include <fstream>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // WIN32

#include "version.h"

void shutdown()
{
    spdlog::shutdown();
}

int main(int argc, char* argv[])
{
    // 解析命令行参数
    Option option(argc, argv);
    int ret = option.Process();
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

    // auto GenNfo = [](int tmdbId, const std::string& nfoPath, const std::string& posterPath) {
    //     std::stringstream sS;
    //     GetMovieDetail(sS, tmdbId);

    //     VideoInfo videoInfo(MOVIE, "");
    //     videoInfo.posterPath = posterPath;
    //     ParseMovieDetailsToVideoDetail(sS, videoInfo.videoDetail);

    //     GetMovieCredits(sS, tmdbId);
    //     ParseCreditsToVideoDetail(sS, videoInfo.videoDetail);

    //     VideoInfoToNfo(videoInfo, nfoPath);
    //     DownloadPoster(videoInfo);
    // };

    // GenNfo(
    //     109987,
    //     "/data-remote/ycnas/新电影/Thief.of.Time.1992.HDTV.1080P.x265.10bit.AAC.Mandarin&Cantonese-FFansTV.nfo",
    //     "/data-remote/ycnas/新电影/Thief.of.Time.1992.HDTV.1080P.x265.10bit.AAC.Mandarin&Cantonese-FFansTV-poster.jpg");

    // return 0;

    // VideoInfo videoInfo(MOVIE, "/data/Movies/Now.You.See.Me.2013.Extended.1080p.BluRay.DTS.x264-NiP.mkv");
    // videoInfo.nfoPath   = "/data/Movies/Now.You.See.Me.2013.Extended.1080p.BluRay.DTS.x264-NiP.nfo";
    // videoInfo.nfoStatus = NFO_FORMAT_MATCH;
    // ParseNfoToVideoInfo(videoInfo);
    // VideoInfoToNfo(videoInfo, "/home/bkzhao/Demo.nfo");
    // return 0;

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

    ApiManager::Instance().SetScanPaths(Config::Instance().GetPaths());

    atexit(shutdown);

    HTTPServerApp app;
    return app.run();
}
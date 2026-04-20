#include "Logger.h"

#include <string>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

std::vector<int> GCrashLogFds;

const size_t INTERLOG_MESSAGE_RESERVED_LINES = 1000; // 内部日志保留的条数

// 保存崩溃日志的路径模板, 采取多路径保存, 尽可能防止因为目录权限问题导致最终全部保存失败
const std::vector<std::string> CRASH_LOGS_PATHS_TEMPLATE = {
    "./ScrapeServer_crash_",
    "/var/log/ScrapeServer_crash_",
    "/tmp/sScrapeServer_crash_",
};

std::vector<std::string> GOpenedCrashLogPaths; // 已打开的崩溃日志路径, 用于正常退出时的日志清理

Logger& Logger::Instance()
{
    static Logger self;
    return self;
}

bool Logger::Init(spdlog::level::level_enum level, bool isDaemon, const std::string& logFile, size_t maxSize, size_t maxCount)
{

    if (m_logger != nullptr) {
        spdlog::critical("Invalid usage for logger: repeated init!");
        return false;
    }

    std::vector<spdlog::sink_ptr> sinks;
    // 给定日志文件名时, 写入日志文件
    if (logFile.empty()) {
        m_stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(m_stdoutSink);
    } else {
        m_fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, maxSize, maxCount, true);
        sinks.push_back(m_fileSink);
    }

    // 注册写入指定缓冲区的日志记录器
    if (isDaemon) {
        m_ringbufferSink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(INTERLOG_MESSAGE_RESERVED_LINES);
        sinks.push_back(m_ringbufferSink);
    }

    m_logger = std::make_shared<spdlog::logger>("logger", std::begin(sinks), std::end(sinks));
    m_logger->set_level(level);
    m_logger->set_pattern("%Y-%m-%d %H:%M:%S %^%L%$ %v [%s:%#]");

    return true;
}

void InitCrashLogFd()
{
    // 格式化本地时间
    time_t    t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    char timeBuf[64] = {0};
    strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tm);
    // 批量拼接时间到模板, 生成最终的日志路径, 打开并存储文件描述符
    for (auto pathTemplate : CRASH_LOGS_PATHS_TEMPLATE) {
        std::string path = pathTemplate + std::string(timeBuf) + ".log";
        int fd = open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
        if (fd != -1) {
            GOpenedCrashLogPaths.push_back(path);
            LOG_INFO("Open crash log {} success, FD: {}", path, fd);
            GCrashLogFds.push_back(fd);
        } else {
            LOG_ERROR("Falied to open crash log {}: {}", path, std::strerror(errno));
        }
    }
    
    // 如果没有发生崩溃, 理应清除已创建的崩溃日志文件
    atexit([]() {
        for (const auto& path : GOpenedCrashLogPaths) {
            unlink(path.c_str());
        }
    });
}

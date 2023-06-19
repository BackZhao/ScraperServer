#include "Logger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

Logger& Logger::Instance()
{
    static Logger self;
    return self;
}

bool Logger::Init(spdlog::level::level_enum level, const std::string& logFile, size_t maxSize, size_t maxCount)
{
    // Debug等级及以下需要打印源代码文件名和行数
    std::string pattern;
    if (level <= spdlog::level::debug) {
        pattern = "%Y-%m-%d %H:%M:%S %^%L%$ %v [%s:%#]";
    } else {
        pattern = "%Y-%m-%d %H:%M:%S %^%L%$ %v";
    }

    if (m_logger != nullptr) {
        spdlog::critical("Invalid usage for logger: repeated init!");
        return false;
    }

    // 给定日志文件名时, 写入日志文件
    if (logFile.empty()) {
        m_logger = spdlog::stdout_color_mt("stdout");
    } else {
        m_logger = spdlog::rotating_logger_mt("file", logFile, maxSize, maxCount, true);
    }

    m_logger->set_level(level);
    m_logger->set_pattern(pattern);

    // FIXME: daemon下无法刷新日志文件

    return true;
}

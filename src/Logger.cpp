#include "Logger.h"

#include <vector>

const size_t INTERLOG_MESSAGE_RESERVED_LINES = 1000; // 内部日志保留的条数

Logger& Logger::Instance()
{
    static Logger self;
    return self;
}

bool Logger::Init(spdlog::level::level_enum level,
                  bool                      isDaemon,
                  const std::string&        logFile,
                  size_t                    maxSize,
                  size_t                    maxCount)
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
    m_logger->set_pattern(pattern);

    return true;
}

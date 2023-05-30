#pragma once

#include <spdlog/spdlog.h>

const size_t DEFAULT_LOGFILE_SIZE  = 5UL * 1024UL * 1024UL; // 默认的日志文件大小, 5MiB
const size_t DEFAULT_LOGFILE_COUNT = 3UL;                   // 默认滚动的日志文件个数, 3个

#define LOG_CRITICAL(...) Logger::Log(__FILE__, __LINE__, spdlog::level::critical, __VA_ARGS__)
#define LOG_ERROR(...) Logger::Log(__FILE__, __LINE__, spdlog::level::err, __VA_ARGS__)
#define LOG_WARN(...) Logger::Log(__FILE__, __LINE__, spdlog::level::warn, __VA_ARGS__)
#define LOG_INFO(...) Logger::Log(__FILE__, __LINE__, spdlog::level::info, __VA_ARGS__)
#define LOG_DEBUG(...) Logger::Log(__FILE__, __LINE__, spdlog::level::debug, __VA_ARGS__)
#define LOG_TRACE(...) Logger::Log(__FILE__, __LINE__, spdlog::level::trace, __VA_ARGS__)

/**
 * @brief 日志记录器
 *
 */
class Logger
{
public:

    /**
     * @brief 单例模式
     *
     * @return Logger& 单例
     */
    static Logger& Instance();

    /**
     * @brief 初始化日志记录器
     *
     * @param level 日志等级
     * @param logFile 保存的日志文件, 留空时不写日志文件(默认为空)
     * @param maxSize 日志文件的大小上限, 默认5MiB
     * @param maxCount 日志文件的滚动存储上限个数, 默认3个
     * @return true 初始化成功
     * @return false 初始化失败
     */
    static bool Init(spdlog::level::level_enum level,
                     const std::string&        logFile  = "",
                     size_t                    maxSize  = DEFAULT_LOGFILE_SIZE,
                     size_t                    maxCount = DEFAULT_LOGFILE_COUNT);

    /**
     * @brief 记录日志
     *
     * @tparam Args 日志参数, 此处用于模板转发参数
     * @param file debug等级以上打印源代码文件名
     * @param line debug等级以上打印源代码行数
     * @param level 日志等级
     * @param fmt 日志格式
     * @param args 日志参数
     */
    template <typename... Args>
    static void Log(const char* file, int line, spdlog::level::level_enum level, const std::string& fmt, Args&&... args)
    {
        m_logger->log(spdlog::source_loc{file, line, ""}, level, fmt, std::forward<Args>(args)...);
    }

private:

    /**
     * @brief 构造函数私有化(单例)
     *
     */
    Logger(){};

    static std::shared_ptr<spdlog::logger> m_logger; // spdlog的日志日志记录器
};

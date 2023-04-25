#include "Option.h"

#include <iostream>

#include <cstdlib>
#include <climits>

#include <version.h>

int ParseOption()
{
    return 0;
}

void Option::DisplayHelp()
{
    /* clang-format off */
	std::cout <<
R"(
Usage:
    supervisor [options] [parameters]

Options:
    -c, --conf <arg>      Set configure file path, default: "/etc/supervisor.json"
    -p, --port <arg>      Set listen port of HTTP server
    -w, --web <arg>       Set the root directory for web resources
    -l, --log-level <arg> Set the log level, default: info, optional input(case insensitive):
                          critical/c, error/e, warn/w, info/i, debug/d, trace/t
    -d, --debug           Decrease the log level from info
    -o, --output <arg>    Set the log saving file path
    -h, --help            Print this help
    -v, --version         Print version of software
        --daemon          Run as daemon

Usage:
    supervisor ctl <control command> [watcher name]

    Use "supervisor ctl -h" to see details for control commands.
)";
    /* clang-format on */
}

bool Option::IsCtlCmd()
{
    std::string appName(m_argv[0]);
    std::string::size_type cmdPos = appName.find("supervisorctl");
    // 当进程的名称不包含"supervisorctl"时
    if (cmdPos == std::string::npos) {
        // 需要两个命令行参数
        if (m_argc < 2) {
            return false;
        }
        // 第二个命令行参数必须是"ctl"
        std::string command(m_argv[1]);
        if (command != "ctl") {
            return false;
        }
    }

    return true;
}

bool Option::IsDaemon()
{
    return m_isDaemon;
}

int Option::ParseLogLevel(const std::string& levelStr)
{
    // 全转大写
    std::string upperLevelStr;
    for (auto ch : levelStr) {
        upperLevelStr += std::toupper(ch);
    }

    if (upperLevelStr == "TRACE" || upperLevelStr == "T") {
        return 0;
    } else if (upperLevelStr == "DEBUG" || upperLevelStr == "D") {
        return 1;
    } else if (upperLevelStr == "INFO" || upperLevelStr == "I") {
        return 2;
    } else if (upperLevelStr == "WARN" || upperLevelStr == "W") {
        return 3;
    } else if (upperLevelStr == "ERROR" || upperLevelStr == "E") {
        return 4;
    } else if (upperLevelStr == "CRITICAL" || upperLevelStr == "C") {
        return 5;
    } else {
        return -1;
    }
}

int Option::Process()
{
    if (IsCtlCmd()) {
        return 1;
    }

    enum {
        OPTION_DAEMON,
    };

    /* clang-format off */
    option options[] = {
        {"daemon",          no_argument, nullptr, OPTION_DAEMON}, // 是否后台
        {"port",      required_argument, nullptr,           'p'}, // 设置端口号
        {"config",    required_argument, nullptr,           'c'}, // 设置配置文件路径
        {"web",       required_argument, nullptr,           'w'}, // 设置前端页面的资源根目录
        {"debug",           no_argument, nullptr,           'd'}, // 降低日志等级
        {"log-level", required_argument, nullptr,           'l'}, // 设置日志等级
        {"output",    required_argument, nullptr,           'o'}, // 设置日志输出文件路径
        {"version",         no_argument, nullptr,           'v'}, // 打印版本号
        {"help",            no_argument, nullptr,           'h'}, // 打印帮助信息
        {nullptr,                     0, nullptr,             0}, // 哨兵元素
    };
    /* clang-format on */

    char pattern[] = "do:l:p:c:w:vh";
    int  opChar    = getopt_long(m_argc, m_argv, pattern, options, nullptr);
    while (opChar != -1) {
        switch (opChar) {
            case OPTION_DAEMON: {
                m_isDaemon = true;
                break;
            }

            case 'd': {
                Config::Instance().DecreaseLogLevel();
                break;
            }

            case 'o': {
                Config::Instance().SetLogFile(optarg);
                break;
            }

            case 'l': {
                int level = ParseLogLevel(optarg);
                if (level < 0) {
                    std::cerr << "Invalid log level:  " << optarg << std::endl;
                    return -1;
                }
                Config::Instance().SetLogLevel(level);
                break;
            }

            case 'p': {
                char* end = NULL;
                long port = strtol(optarg, &end, 0);
                if (end == optarg || *end != '\0' || port <= 0 || port >= USHRT_MAX) {
                    std::cerr << "Invalid port number " << optarg << std::endl;
                    return -1;
                }
                Config::Instance().SetPort(port);
                break;
            }

            case 'c': {
                Config::Instance().SetConfFile(optarg);
                break;
            }

            case 'v': {
                std::cout << VERSION << std::endl;
                return 0;
            }

            case 'h': {
                DisplayHelp();
                return 0;
            }

            case '?':
            default: {
                DisplayHelp();
                return 0;
            }
        }
        opChar = getopt_long(m_argc, m_argv, pattern, options, nullptr);
    }

    // 出现无法解析的命令行选项
    if (optind < m_argc) {
        while (optind < m_argc) {
            std::cerr << "Invalid option: " << m_argv[optind++] << std::endl;
        }
        DisplayHelp();
        return -1;
    }

    return 1;
}

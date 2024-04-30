#pragma once

#include "Config.h"

class Option
{
public:

    /**
     * @brief 构造函数
     *
     * @param argc
     * @param argv
     * @param appConf
     */
    Option(int argc, char* argv[]) : m_argc(argc), m_argv(argv), m_isDaemon(false){};

    /**
     * @brief 处理命令行选项
     *
     * @return int
     */
    int Process();

    /**
     * @brief 是否设置了后台运行
     *
     * @return true 是
     * @return false 否
     */
    bool IsDaemon();

private:

    /**
     * @brief 打印帮助信息
     *
     */
    void DisplayHelp();

private:

    int    m_argc;
    char** m_argv;
    bool   m_isDaemon; // 是否设置了后台运行
};

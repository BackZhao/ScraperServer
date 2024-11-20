#pragma once

#include <functional>

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Object.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>

class Control
{
    /**
     * @brief 控制命令的类型
     *
     */
    enum CtlCmdType {
        QUIT_SERVER,  // 停止服务端
        DISPLAY_HELP, // 打印帮助信息
        INTER_LOG,    // 获取内部日志
    };

public:

    /**
     * @brief Control类构造函数
     *
     * @param ctlCmd 要执行控制的命令和watcher名称
     */
    Control(int argc, char* argv[]) : m_argc(argc), m_argv(argv), m_host("127.0.0.1"), m_port(54250){};

    /**
     * @brief 判断是否为控制命令
     *
     * @param argc 命令行参数个数
     * @param argv 命令行参数
     * @return true 是
     * @return false 否
     */
    static bool isCtlCmd(int argc, char* argv[]);

    /**
     * @brief 处理控制命令
     *
     * @return true 执行成功
     * @return false 执行失败
     */
    bool ProcessCtl();

private:

    /**
     * @brief 显示帮助信息
     *
     */
    void DisplayHelp();

    /**
     * @brief 解析命令行参数
     *
     * @return true 解析成功
     * @return false 解析失败
     */
    bool ParseCmd();

    /**
     * @brief 初始化类中的部分成员变量
     *
     * @return true 初始化成功
     * @return false 初始化失败
     */
    bool InitMembers();

    /**
     * @brief 判断是否需要鉴权
     * 
     * @return true 是
     * @return false 否
     */
    bool NeedAuth();

    /**
     * @brief 通过API的Digist鉴权
     *
     * @return true 鉴权成功
     * @return false 鉴权失败
     */
    bool DigestAuth();

    /**
     * @brief 初始化类
     *
     * @return true
     * @return false
     */
    bool Init();

    /**
     * @brief 发送HTTP请求, 获取响应并解析为JSON对象
     *
     * @return true
     * @return false
     */
    bool SendRequest();

    /**
     * @brief 获取服务端内部日志
     *
     * @return true 获取成功
     * @return false 获取失败
     */
    bool InterLog();

    /**
     * @brief 退出服务端
     * 
     * @return true 成功
     * @return false 失败
     */
    bool QuitServer();

private:

    int                          m_argc;        // 进程命令行参数个数
    char**                       m_argv;        // 进程命令行参数列表
    std::string                  m_host;        // 服务端主机名
    uint16_t                     m_port;        // 服务端端口号
    std::vector<std::string>     m_ctlCmd;      // 控制名参数
    CtlCmdType                   m_cmdType;     // 要执行控制的命令
    Poco::URI                    m_uri;         // 需要访问的api地址
    Poco::Net::HTTPClientSession m_session;     // HTTP请求会话
    Poco::Net::HTTPRequest       m_request;     // HTTP请求
    Poco::Net::HTTPResponse      m_response;    // HTTP响应
    std::function<bool(void)>    m_processFunc; // 用于保存执行命令的对应函数地址
    Poco::Dynamic::Var           m_parseResult; // HTTP响应解析成的JSON对象
};

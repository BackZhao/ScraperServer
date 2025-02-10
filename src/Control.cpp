#include "Control.h"

#include "Logger.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <string>

#include <getopt.h>

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/URI.h>

bool GTailStopped; // 日志跟踪命令是否停止

bool Control::isCtlCmd(int argc, char* argv[])
{
    if (argc < 2) {
        return false;
    }

    // 进程的名称包含"supervisorctl"或者参数中包含"ctl"
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "ctl") {
            return true;
        }
    }

    return false;
}

bool Control::InitMembers()
{
    if (m_cmdType == DISPLAY_HELP) {
        DisplayHelp();
        return false;
    }

    if (!Logger::Instance().Init(spdlog::level::info)) {
        std::cerr << "Logger init failed!" << std::endl;
        return false;
    }

    // 根据要执行的命令类型, 拼接出对应的HTTP地址
    std::ostringstream oSS;
    oSS << "http://" << m_host << ":" << m_port << "/api/";
    std::string uriStr     = oSS.str();
    switch (m_cmdType) {
        case QUIT_SERVER: {
            uriStr += "quit";
            m_processFunc = std::bind(&Control::QuitServer, this);
            break;
        }

        case INTER_LOG: {
            uriStr += "interlog";
            m_processFunc = std::bind(&Control::InterLog, this);
            break;
        }

        default: {
            LOG_ERROR("Invalid control command! value: {}", m_cmdType);
            return false;
        }
    }
    m_uri = uriStr;
    return true;
}

bool Control::NeedAuth()
{
    switch (m_cmdType) {
        case QUIT_SERVER:
        case INTER_LOG:
            return true;
        case DISPLAY_HELP:
            return false;
        default:
            return false;
    }
}

bool Control::DigestAuth()
{
    std::string path(m_uri.getPathAndQuery());
    if (path.empty()) {
        path = "/";
    }
    m_session.setHost(m_uri.getHost());
    m_session.setPort(m_uri.getPort());
    m_request.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
    m_request.setURI(path);
    m_request.setVersion(Poco::Net::HTTPMessage::HTTP_1_1);

    // Digest鉴权第一次握手, 服务端填写部分鉴权参数
    try {
        m_session.sendRequest(m_request);
    } catch (Poco::Exception& e) {
        LOG_ERROR("Digest auth request send failed: {}", e.displayText());
        return false;
    }
    m_session.receiveResponse(m_response);
    if (m_response.getStatus() == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED) {
        // 在HTTP请求头中填入用户名和密码
        Poco::Net::HTTPDigestCredentials crediential("admin", "Ss@@bkzhao97");
        crediential.authenticate(m_request, m_response);
        return true;
    } else {
        LOG_ERROR("Digest auth failed!\nServer response:\n\tCode: {}\n\tReason:{}",
                  m_response.getStatus(),
                  m_response.getReason());
        return false;
    }
}

bool Control::Init()
{
    if (!InitMembers()) {
        return false;
    }

    if (NeedAuth()) {
        if (!DigestAuth()) {
            return false;
        }
    }

    return true;
}

bool Control::SendRequest()
{
    try {
        m_session.sendRequest(m_request);
    } catch (Poco::Exception& e) {
        LOG_ERROR("Send request to {} failed: {}", m_uri.toString(), e.displayText());
        LOG_ERROR("Please check whether the supervisor server is running!");
        return false;
    }

    std::istream& rs = m_session.receiveResponse(m_response);
    if (m_response.getStatus() == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED) {
        LOG_ERROR("Invalid username or password!");
        return false;
    } else if (m_response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        LOG_ERROR("Response return error!");
        LOG_ERROR("Response Body:");
        Poco::StreamCopier::copyStream(rs, std::cerr);
        std::cerr << std::endl;
        return false;
    } else {
        // 防止响应不是JSON格式
        try {
            Poco::JSON::Parser parser;
            m_parseResult = parser.parse(rs);
        } catch (Poco::Exception& e) {
            LOG_ERROR("The response parsed failed as json: {}", e.displayText());
            LOG_ERROR("Response Body:");
            Poco::StreamCopier::copyStream(rs, std::cerr);
            std::cerr << std::endl;
            return false;
        }

        auto jsonPtr = m_parseResult.extract<Poco::JSON::Object::Ptr>();
        // 服务端API请求返回判断
        if (!jsonPtr->optValue("success", false)) {
            LOG_ERROR("Server return error:\n {}", jsonPtr->optValue<std::string>("msg", ""));
            return false;
        }

        return true;
    }
}

void Control::DisplayHelp()
{
    /* clang-format off */
    std::cerr <<
R"(
Usage:
    )" << m_argv[0] << R"( [ctl] [options] <control command> [watcher name] [options]

Options:
    -p, --port <arg>   Set server port, default: 8000
    -h, --host <arg>   Set server host, default: 127.0.0.1

Control Commands:
    quit        notify the server to quit
    interlog    print the log output for internal/self(max size is 1000 lines)
    help        print this help message

)";
    /* clang-format on */
}

bool Control::ParseCmd()
{
    /* clang-format off */
    option options[] = {
        {"host",      required_argument, nullptr,           'h'}, // 设置端口号
        {"port",      required_argument, nullptr,           'p'}, // 设置端口号
        {nullptr,                     0, nullptr,             0}, // 哨兵元素
    };
    /* clang-format on */

    char pattern[] = "h:p:";
    int  opChar    = getopt_long(m_argc, m_argv, pattern, options, nullptr);
    while (opChar != -1) {
        switch (opChar) {
            case 'h': {
                std::cout << "Using custom server host: " << optarg << std::endl;
                m_host = optarg;
                break;
            }

            case 'p': {
                char* end  = NULL;
                long  port = strtol(optarg, &end, 0);
                if (end == optarg || *end != '\0' || port <= 0 || port >= USHRT_MAX) {
                    std::cerr << "Invalid port number " << optarg << std::endl;
                    return false;
                }
                std::cout << "Using custom server port: " << port << std::endl;
                m_port = static_cast<uint16_t>(port);
                break;
            }

            case '?':
            default: {
                DisplayHelp();
                return 0;
            }
        }
        opChar = getopt_long(m_argc, m_argv, pattern, options, nullptr);
    }

    if (std::string(m_argv[0]).find("supervisorctl") != std::string::npos) {
        m_ctlCmd.push_back("ctl");
    }

    if (optind < m_argc) {
        while (optind < m_argc) {
            m_ctlCmd.push_back(m_argv[optind++]);
        }
    }

    if (m_ctlCmd.size() < 2) {
        std::cerr << "No control command given!" << std::endl;
        DisplayHelp();
        return false;
    }

    /* clang-format off */
    // 映射控制命令的字符串和枚举值
    static std::map<std::string, CtlCmdType> ctlTypeMap = {
        {"quit",     QUIT_SERVER},
        {"help",    DISPLAY_HELP},
        {"interlog",    INTER_LOG},
    };
    /* clang-format on */

    std::string ctlCmd(m_ctlCmd[1]);
    auto        findResult = ctlTypeMap.find(ctlCmd);
    if (findResult == ctlTypeMap.end()) {
        std::cerr << "Invalid control command: " << ctlCmd << std::endl;
        DisplayHelp();
        return false;
    } else {
        m_cmdType = findResult->second;
    }

    return true;
}

bool Control::ProcessCtl()
{
    if (!ParseCmd()) {
        return false;
    }

    if (!Init()) {
        return false;
    }

    return m_processFunc();
}

bool Control::QuitServer()
{
    if (!SendRequest()) {
        return false;
    }

    LOG_INFO("Server is quitting...");
    return true;
}

bool Control::InterLog()
{
    if (!SendRequest()) {
        return false;
    }

    auto jsonPtr    = m_parseResult.extract<Poco::JSON::Object::Ptr>();
    auto logArrPtr = jsonPtr->getArray("interlog");
    for (size_t i = 0 ; i < logArrPtr->size(); i++) {
        std::cout << logArrPtr->get(i).toString();
    }
    std::cout << std::endl;
    return true;
}
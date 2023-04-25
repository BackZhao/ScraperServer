#pragma once

#include <Poco/JSON/Object.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/URI.h>

using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;

/**
 * @brief 处理非法HTTP请求的类
 *
 */
class InvalidRequestHandler : public HTTPRequestHandler
{
public:

    InvalidRequestHandler() {}

    /**
     * @brief
     *
     * @param request
     * @param response
     */
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response);
};

/**
 * @brief 处理"/api/"请求的类
 *
 */
class ApiRequestHandler : public HTTPRequestHandler
{
public:

    ApiRequestHandler() {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response);

private:

    /**
     * @brief 将HTTP的查询参数转换成JSON格式, 方便处理
     *
     * @param queryParam HTTP查询参数
     * @return Poco::JSON::Object
     */
    Poco::JSON::Object QueryParamToJson(const Poco::URI::QueryParameters& queryParam);
};

/**
 * @brief 处理首页访问请求的类
 *
 */
class IndexRequestHandler : public HTTPRequestHandler
{
public:

    IndexRequestHandler() {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response);
};

/**
 * @brief 处理资源请求的类
 *
 */
class ResRequestHandler : public HTTPRequestHandler
{
private:

    /**
     * @brief 获取MIME类型字符串
     *
     * @param fileName 文件名
     * @return std::string MIME类型字符串
     */
    static std::string MimeType(const std::string& fileName);

public:

    ResRequestHandler() {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response);
};

/**
 * @brief 首次访问, 提示客户端进行Basic鉴权的工厂类
 * 
 */
class AuthHandler : public HTTPRequestHandler
{
public:

    AuthHandler() {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response);
};

/**
 * @brief 生成HTTP请求处理类的工厂类
 *
 */
class SupervisorRequestHandlerFactory : public HTTPRequestHandlerFactory
{
public:

    /**
     * @brief Construct a new Supervisor Request Handler Factory object
     *
     */
    SupervisorRequestHandlerFactory() {}

    /**
     * @brief Create a Request Handler object
     *
     * @param request
     * @return HTTPRequestHandler*
     */
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request);

private:

    /**
     * @brief 鉴权
     * 
     * @param request HTTP请求
     * @return HTTPRequestHandler* 鉴权通过则返回nullptr
     */
    HTTPRequestHandler* CheckAuth(const HTTPServerRequest& request);
};

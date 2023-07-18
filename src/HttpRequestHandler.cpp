#include "HttpRequestHandler.h"

#include <functional>
#include <utility>
#include <fstream>

#include <Poco/File.h>
#include <Poco/FileStream.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/StreamCopier.h>
#include <Poco/Thread.h>
#include <Poco/URI.h>

#include "ApiManager.h"
#include "Config.h"
#include "Logger.h"

void InvalidRequestHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    LOG_TRACE("Invalid request from {}", request.clientAddress().toString());

    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");

    std::ostream& ostr = response.send();
    Poco::JSON::Object jsonObj;
    jsonObj.set("err", "Invalid HTTP request!");
    jsonObj.stringify(ostr);
}

Poco::JSON::Object ApiRequestHandler::QueryParamToJson(const Poco::URI::QueryParameters& queryParam)
{
    Poco::JSON::Object queryJsonObj;
    for (const auto& iter : queryParam) {
        if (iter.second.empty()) {
            queryJsonObj.set(iter.first, true);
        } else {
            queryJsonObj.set(iter.first, iter.second);
        }
    }
    return queryJsonObj;
}

void ApiRequestHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    LOG_TRACE("Api request from {}", request.clientAddress().toString());

    response.setChunkedTransferEncoding(true);
    response.setContentType("application/json");
    std::ostream& ostr = response.send();

    // 关联API与后端接口
    using namespace std::placeholders;
    static std::map<std::string, ApiManager::ApiHandler> RegApiHandlers = {
        {"/api/scan", std::bind(&ApiManager::Scan, &ApiManager::Instance(), _1, _2)},
        {"/api/scanResult", std::bind(&ApiManager::ScanResult, &ApiManager::Instance(), _1, _2)},
        {"/api/list", std::bind(&ApiManager::List, &ApiManager::Instance(), _1, _2)},
        {"/api/detail", std::bind(&ApiManager::Detail, &ApiManager::Instance(), _1, _2)},
        {"/api/scrape", std::bind(&ApiManager::Scrape, &ApiManager::Instance(), _1, _2)},
        {"/api/refresh", std::bind(&ApiManager::Refresh, &ApiManager::Instance(), _1, _2)},
        {"/api/refreshResult", std::bind(&ApiManager::RefreshResult, &ApiManager::Instance(), _1, _2)},
    };

    Poco::URI uri(request.getURI());
    auto iter = RegApiHandlers.find(uri.getPath());
    if (iter != RegApiHandlers.end()) {
        iter->second(QueryParamToJson(uri.getQueryParameters()), ostr);
    } else { // 非法API请求
        ostr << R"({"err": "Invalid API request!"})";
    }
}

void IndexRequestHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    LOG_TRACE("Index request from {}", request.clientAddress().toString());

    response.setChunkedTransferEncoding(true);
    std::string indexFile = "/index.html";
    if (!Poco::File(indexFile).exists()) {
        std::ostream& ostr = response.send();
        ostr << "Invalid resource request!";
        return;
    }
    response.sendFile(indexFile, "text/html");
}

std::string ResRequestHandler::MimeType(const std::string& fileName)
{
    auto dotPos = fileName.find_last_of(".");
    if (dotPos == std::string::npos || dotPos == fileName.size() - 1) {
        return "text/plain";
    }

    std::string extStr = fileName.substr(dotPos + 1, fileName.size() - dotPos - 1);
    /* clang-format off */
    static std::map<std::string, std::string> extToMime = {
        {"txt",   "text/plain"},
        {"xml",   "text/xml"},
        {"xslt",  "text/xml"},
        {"xsl",   "text/xml"},
        {"html",  "text/html"},
        {"htm",   "text/html"},
        {"shtm",  "text/html"},
        {"shtml", "text/html"},
        {"css",   "text/css"},
        {"ttf",   "application/x-font-ttf"},
        {"js",    "application/x-javascript"},
        {"ico",   "image/x-icon"},
        {"icon",  "image/x-icon"},
        {"gif",   "image/gif"},
        {"jpg",   "image/jpeg"},
        {"jpeg",  "image/jpeg"},
        {"png",   "image/png"},
        {"svg",   "image/svg+xml"},
    };
    /* clang-format on */

    auto iter = extToMime.find(extStr);
    if (iter != extToMime.end()) {
        return iter->second;
    } else {
        return "text/plain";
    }
}

void ResRequestHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    LOG_TRACE("Static res request from {}", request.clientAddress().toString());

    std::string fileName = request.getURI();
    if (!Poco::File(fileName).exists()) {
        std::ostream& ostr = response.send();
        ostr << "Invalid resource request!";
        return;
    }
    response.sendFile(fileName, MimeType(fileName));
}

void AuthHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    response.requireAuthentication("Supervisor");
    response.send();
}

HTTPRequestHandler* SupervisorRequestHandlerFactory::CheckAuth(const HTTPServerRequest& request)
{
    // 首次访问, 提示进行Basic鉴权
    if (request.find("Authorization") == request.end()) {
        return new AuthHandler;
    }

    // 校验用户名密码(admin:kaida)
    if (request.get("Authorization") != "Basic YWRtaW46a2FpZGE=") {
        return new AuthHandler;
    }

    return nullptr;
}

HTTPRequestHandler* SupervisorRequestHandlerFactory::createRequestHandler(const HTTPServerRequest& request)
{
    const std::string& uri = request.getURI();
    if (uri.find("/api/") == 0) {
        return new ApiRequestHandler();
    } else {
        auto checkResult = CheckAuth(request);
        if (uri == "/") {
            return checkResult == nullptr ? new IndexRequestHandler() : checkResult;
        } else {
            return checkResult == nullptr ? new ResRequestHandler() : checkResult;
        }
    }
}

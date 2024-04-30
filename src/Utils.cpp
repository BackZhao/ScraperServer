#include "Utils.h"

#include <Poco/JSON/Object.h>

void FillWithResponseJson(std::ostream &out, bool isSuccess, const std::string &msg)
{
    Poco::JSON::Object jsonObj;
    jsonObj.set("success", isSuccess);
    if (!msg.empty()) {
        jsonObj.set("msg", msg);
    }

    jsonObj.stringify(out);
}

std::size_t ReplaceString(std::string& inout, const std::string& what, const std::string& with)
{
    std::size_t count{};
    for (std::string::size_type pos{}; inout.npos != (pos = inout.find(what.data(), pos, what.length()));
         pos += with.length(), ++count) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
}

int ParseLogLevel(const std::string& levelStr)
{
    // 全转大写
    std::string upperLevelStr;
    for (auto ch : levelStr) {
        upperLevelStr += std::toupper(ch);
    }

    static std::map<std::string, int> str2LevelMap = {
        {"TRACE", 0},    {"T", 0},
        {"DEBUG", 0},    {"D", 1},
        {"INFO", 0},     {"I", 2},
        {"ERROR", 0},    {"E", 3},
        {"CRITICAL", 0}, {"C", 4},
    };

    auto iter = str2LevelMap.find(upperLevelStr);
    return iter == str2LevelMap.end() ? -1 : iter->second;
}
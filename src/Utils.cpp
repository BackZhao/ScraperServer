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
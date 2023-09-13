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
#pragma once

#include <iostream>
#include <string>

void FillWithResponseJson(std::ostream &out, bool isSuccess, const std::string &msg = "");

std::size_t ReplaceString(std::string& inout, const std::string& what, const std::string& with);

int ParseLogLevel(const std::string& levelStr);
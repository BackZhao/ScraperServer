#pragma once

#include <map>
#include <string>

class ResourceManager
{
public:

    static ResourceManager& Instance();

    const char* GetData(const std::string& fileName, size_t& fileSize);

    bool Init();

private:

    ResourceManager(){};

private:

    std::map<std::string, std::string> m_data;
};
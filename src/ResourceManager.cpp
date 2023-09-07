#include "ResourceManager.h"

#include <iostream>
#include <sstream>

#include <Poco/FileStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/Zip/ZipArchive.h>
#include <Poco/Zip/ZipStream.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tchar.h>
static char*  res_start = nullptr;
static size_t res_size  = 0;
#else
extern char _binary_res_zip_start;
extern char _binary_res_zip_end;
#endif

ResourceManager& ResourceManager::Instance()
{
    static ResourceManager inst;
    return inst;
}

bool ResourceManager::Init()
{
#if defined _WIN32
    HRSRC hR = ::FindResource(NULL, _T("IDR_RES_ZIP"), RT_RCDATA);
    if (!hR) return false;

    res_size  = ::SizeofResource(NULL, hR);
    res_start = new char[res_size];
    if (!res_start) return false;

    HGLOBAL hG = (LPBYTE)::LoadResource(NULL, hR);
    if (!hG) return false;

    LPBYTE lpData = (LPBYTE)::LockResource(hG);
    memcpy(res_start, lpData, res_size);
    ::FreeResource(hG);

    char*  dataBegin = res_start;
    size_t dataSize  = res_size;
#else
    char*  dataBegin = &_binary_res_zip_start;
    size_t dataSize  = &_binary_res_zip_end - &_binary_res_zip_start;
#endif // _WIN32

    std::string        str(dataBegin, dataSize);
    std::istringstream iSS(str);

    try {
        Poco::Zip::ZipArchive zip(iSS);
        iSS.clear();
        iSS.seekg(0);
        for (auto it = zip.headerBegin(); it != zip.headerEnd(); ++it) {
            Poco::Zip::ZipInputStream zipis(iSS, it->second);
            // 仅需解压保存文件, 跳过目录
            Poco::Path path(it->second.getFileName());
            if (path.isFile()) {
                std::ostringstream out(std::ios::binary);
                Poco::StreamCopier::copyStream(zipis, out);
                m_data[it->first] = out.str();
            }
        }
    } catch (Poco::Exception& e) {
        std::cerr << "Decompress zip data for webui failed: " << e.displayText() << std::endl;
    }

    return true;
}

const char* ResourceManager::GetData(const std::string& fileName, size_t& fileSize)
{
    auto iter = m_data.find(fileName);
    if (iter == m_data.end()) {
        return nullptr;
    } else {
        fileSize = iter->second.size();
        return &(iter->second[0]);
    }
}
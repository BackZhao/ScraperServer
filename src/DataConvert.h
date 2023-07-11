#pragma once

#include "CommonType.h"

#include <Poco/JSON/Object.h>


// TODO: 接口设计上优化形参, 部分接口设置不够合理

void VideoInfoToBriefJson(uint32_t id, const VideoInfo& videoInfo, Poco::JSON::Object& outJson);

void VideoInfoToDetailedJson(uint32_t id, const VideoInfo& videoInfo, Poco::JSON::Object& outJson);

/**
 * @brief 
 * 
 * @param videoInfo 
 * @param nfoPath 
 * @return true 
 * @return false 
 */
bool VideoInfoToNfo(const VideoInfo& videoInfo, const std::string& nfoPath, bool setHDRTitle);

/**
 * @brief 解析NFO文件
 * 
 * @param videoInfo 保存解析结果的结构体引用
 * @return true 解析成功
 * @return false 解析失败
 */
bool ParseNfoToVideoInfo(VideoInfo& videoInfo);

bool ParseMovieDetailsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail);

bool ParseCreditsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail);

/**
 * @brief 
 * 
 * @param jsonObj 
 * @return true 
 * @return false 
 */
bool WriteEpisodeNfo(const Poco::JSON::Array::Ptr jsonArrPtr, const std::vector<std::string>& episodePaths);

bool SetTVEnded(const std::string& nfoPath);


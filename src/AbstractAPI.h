#pragma once

/**
 * @file API访问的抽象类
 * @author backzhao@qq.com
 * @brief
 * @version 0.1
 * @date 2023-09-12
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "CommonType.h"

class AbstractAPI
{
public:

    // virtual bool SearchTV(VideoType VideoType, const std::string& keywords, const std::string& year = "") = 0;
    // virtual bool SearchMovie(VideoType VideoType, const std::string& keywords, const std::string& year = "") = 0;

    /**
     * @brief 刮削电影
     * 
     * @param videoInfo 需要填写的结构体
     * @param movieID 电影的ID
     * @return true 刮削成功
     * @return false 刮削失败
     */
    virtual bool ScrapeMovie(VideoInfo& videoInfo, int movieID) = 0;

    /**
     * @brief 刮削电视剧
     * 
     * @param videoInfo 需要填写的结构体
     * @param tvId 电视剧的ID
     * @param seasonId 电视剧的季编号
     * @return true 刮削成功
     * @return false 刮削失败
     */
    virtual bool ScrapeTV(VideoInfo& videoInfo, int tvId, int seasonId) = 0;

    /**
     * @brief 获取最后一次的错误码(enum类型, 建议使用GetLastErrStr()接口)
     *
     * @return int 错误码
     */
    virtual int GetLastErrCode() = 0;

    /**
     * @brief 获取最后一次错误字符串
     * 
     * @return const std::string& 错误字符串
     */
    virtual const std::string& GetLastErrStr() = 0;
};
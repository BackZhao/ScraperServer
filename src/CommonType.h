#pragma once

#include <map>
#include <string>
#include <vector>

const std::vector<std::string> VIDEO_SUFFIX = {"mkv", "mp4", "iso", "ts"}; // 视频文件的后缀名集合

/**
 * @brief 视频类别
 *
 */
enum VideoType {
    MOVIE,        // 电影
    MOVIE_SET,    // 电影集
    TV,           // 电视剧
    UNKNOWN_TYPE, // 位置类型
};

/**
 * @brief 元数据文件的状态(图片, NFO文件等)
 * 
 */
enum MetaFileStatus {
    FILE_FORMAT_MATCH,    // 文件格式匹配
    FILE_FORMAT_MISMATCH, // 文件格式不匹配
    FILE_NOT_FOUND,       // 文件不存在
};

/**
 * @brief 视频的HDR类型
 * 
 */
enum HDRType {
    NON_HDR,                // 非HDR类型
    HDR10,                  // HDR10
    HDR10Plus,              // HDR10+
    DOLBY_VISION,           // 杜比视界
    DOLBY_VISION_AND_HDR10, //杜比视界 + HDR10
};

/**
 * @brief 视频文件类型(是否在目录内, 主要针对电影)
 * 
 */
enum VideoFileType {
    NO_FOLDER, // 没有上层目录
    IN_FOLDER, // 包含上层目录
};

static std::map<std::string, VideoType> STR_TO_VIDEO_TYPE = {
    {"movie", MOVIE},
    {"tv", TV},
    {"movieSet", MOVIE_SET},
};

static std::map<VideoType, std::string> VIDEO_TYPE_TO_STR = {
    {MOVIE, "movie"},
    {TV, "tv"},
    {MOVIE_SET, "movieSet"},
};

/**
 * @brief 演员详情
 *
 */
struct ActorDetail {
    std::string name;  // 演员名称
    std::string role;  // 演员扮演的角色
    int         order; // 演员的排序顺序
    std::string thumb; // 演员的预览图
};

/**
 * @brief 用户评分
 *
 */
struct RatingDetail {
    double rating; // 评分
    int    votes;  // 投票人数
};

/**
 * @brief 电视剧详情
 *
 */
struct EpisodeDetail {
    int         episodeNumber; // 电视剧的集数
    std::string fullPath;      // 电视剧的全路径
    std::string title;         // 电视剧的标题
    std::string plot;          // 电视剧的剧情
};

/**
 * @brief 季详情
 *
 */
struct SeasonDetail {
    bool                       queryed;      // 是否查询过
    std::string                air_date;     // 季的播出日期
    std::string                name;         // 季的名称
    std::string                plot;         // 季的剧情
    int                        episodeCount; // 季的电视剧总数
    int                        seasonNumber; // 季的编号
    std::string                posterPath; // 季的海报图片地址(短地址, 仅包含服务器上的文件名称)
    std::vector<EpisodeDetail> episodeDetails; // 季的所有电视剧详情
};

/**
 * @brief 公共详情
 *
 */
struct VideoDetail {
    VideoDetail() : episodeNfoCount(0) {}

    std::string                title;         // 视频的标题
    std::string                originaltitle; // 视频的原始标题
    RatingDetail               ratings;       // 视频的用户评分
    std::string                plot;          // 视频的剧情介绍
    std::map<std::string, int> uniqueid;      // 不同API类型下, 视频的唯一ID
    std::vector<std::string>   genre;         // 视频的分类
    std::vector<std::string>   countries;     // 视频的国家
    std::vector<std::string>   credits;       // 视频的编剧
    std::string                director;      // 视频的导演
    std::string                premiered;     // 视频的首播日期
    std::vector<std::string>   studio;        // 视频的制片厂
    std::vector<ActorDetail>   actors;        // 视频的演员信息

    // 电视剧专属
    int                      seasonNumber;    // 季编号
    size_t                   episodeNfoCount; // 电视剧剧集完整的NFO文件个数
    std::vector<std::string> episodePaths;    // 电视剧剧集路径
    bool                     isEnded;         // 是否已完结

    std::string posterUrl;    // 视频的海报图片地址(短地址, 仅包含服务器上的文件名称)
    std::string fanartUrl;    // 剧照的图片地址(短地址, 仅包含服务器上的文件名称)
    std::string clearLogoUrl; // 标志的图片地址(短地址, 仅包含服务器上的文件名称)
};

/**
 * @brief 视频信息
 *
 */
struct VideoInfo {
    VideoInfo(VideoType type, const std::string& path) : videoType(type), videoPath(path) {}

    VideoType     videoType; // 视频类型
    std::string   videoPath; // 视频所在路径
    HDRType       hdrType;   // HDR的类型
    VideoFileType videoFiletype;

    MetaFileStatus nfoStatus;       // NFO文件状态
    MetaFileStatus posterStatus;    // 海报文件状态
    MetaFileStatus fanartStatus;    // 剧照文件状态
    MetaFileStatus clearlogoStatus; // 标志文件状态

    std::string nfoPath;       // NFO文件路径
    std::string posterPath;    // 海报路径
    std::string fanartPath;    // 剧照路径
    std::string clearlogoPath; // logo路径

    VideoDetail videoDetail;
};

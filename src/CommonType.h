#pragma once

#include <string>
#include <vector>
#include <map>

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
 * @brief NFO文件状态
 *
 */
enum NfoStatus {
    NFO_FORMAT_MATCH,    // NFO文件格式匹配(KODI格式)
    NFO_FORMAT_MISMATCH, // NFO文件格式不匹配(KODI格式)
    NFO_NOT_FOUND,       // NFO文件不存在
};

/**
 * @brief 海报文件状态
 *
 */
enum PosterStatus {
    POSTER_COMPELETED,   // 海报文件完整(JPEG格式)
    POSTER_INCOMPELETED, // 海报文件不完整(JPEG格式)
    POSTER_NOT_FOUND,    // 海报文件不存在
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

static std::map<NfoStatus, std::string> NFO_STATUS_TO_STR = {
    {NFO_FORMAT_MATCH, "Nfo format match"},
    {NFO_FORMAT_MISMATCH, "Nfo format mismatch"},
    {NFO_NOT_FOUND, "Nfo not found"},
};

static std::map<PosterStatus, std::string> POSTER_STATUS_TO_STR = {
    {POSTER_COMPELETED, "Poster completed"},
    {POSTER_INCOMPELETED, "Poster incompleted"},
    {POSTER_NOT_FOUND, "Poster not found"},
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
    std::vector<uint8_t>       posterData;  // 季的海报图片二进制数据
    std::vector<EpisodeDetail> episodeDetails; // 季的所有电视剧详情
};

/**
 * @brief 公共详情
 *
 */
struct VideoDetail {
    VideoDetail() : episodeNfoCount(0) {}

    std::string              title;         // 视频的标题
    std::string              originaltitle; // 视频的原始标题
    RatingDetail             userrating;    // 视频的用户评分
    std::string              plot;          // 视频的剧情介绍
    int                      uniqueid;      // 视频的唯一ID, 这里指定为TMDB的ID
    std::vector<std::string> genre;         // 视频的分类
    std::vector<std::string> countries;     // 视频的国家
    std::vector<std::string> credits;       // 视频的编剧
    std::string              director;      // 视频的导演
    std::string              premiered;     // 视频的首播日期
    std::string              studio;        // 视频的制片厂
    std::vector<ActorDetail> actors;        // 视频的演员信息

    // 电视剧专属
    int                      seasonNumber;    // 季编号
    int                      episodeNfoCount; // 电视剧剧集完整的NFO文件个数
    std::vector<std::string> episodePaths;    // 电视剧剧集路径
    bool                     isEnded;         // 是否已完结
};

/**
 * @brief 视频信息
 *
 */
struct VideoInfo {
    VideoInfo(VideoType type, const std::string& path) : videoType(type), videoPath(path) {}

    VideoType            videoType;
    std::string          videoPath;
    NfoStatus            nfoStatus;    // NFO文件状态
    PosterStatus         posterStatus; // 海报文件状态
    std::string          nfoPath;      // NFO文件路径
    std::string          posterPath;   // 海报路径
    std::string          posterUrl;    // 视频的海报图片地址(短地址, 仅包含服务器上的文件名称)
    std::vector<uint8_t> posterData;   // 视频海报的图片二进制数据
    VideoDetail          videoDetail;
};

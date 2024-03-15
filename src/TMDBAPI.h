#pragma once

#include "AbstractAPI.h"

#include <Poco/URI.h>

class TMDBAPI : public AbstractAPI
{
    enum ErrCode {
        NO_ERR,
        GET_MOVIE_DETAIL_FAILED,
        GET_TV_DETAIL_FAILED,
        GET_SEASON_DETAIL_FAILED,
        GET_TV_CREDITS_FAILED,
        GET_MOVIE_CREDITS_FAILED,
        PARSE_MOVIE_DETAIL_FAILED,
        PARSE_TV_DETAIL_FAILED,
        PARSE_SEASON_DETAIL_FAILED,
        PARSE_CREDITS_FAILED,
        DOWNLOAD_POSTER_FAILED,
        WRITE_NFO_FILE_FAILED,
    };

public:

    bool ScrapeMovie(VideoInfo& videoInfo, int movieID) override;

    bool ScrapeTV(VideoInfo& videoInfo, int tvId, int seasonId) override;

    bool UpdateTV(VideoInfo& videoInfo);

    int GetLastErrCode() override;

    const std::string& GetLastErrStr() override;

private:

    bool IsImagesAllFilled(const VideoDetail& videoDetail);

    bool SendRequest(std::ostream& out, const Poco::URI& uri);

    bool ParseMovieDetailsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail);

    bool ParseTVDetailsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail, int seasonId);

    bool ParseCreditsToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail);

    bool ParseImagesToVideoDetail(std::stringstream& sS, VideoDetail& videoDetail);

    bool GetMovieDetail(int tmdbid, VideoDetail& videoDetail);

    bool GetTVDetail(int tmdbId, int seasonId, VideoDetail& videoDetail);

    bool GetSeasonDetail(int tmdbId, int seasonNum, VideoDetail& videoDetail);

    bool GetMovieCredits(int tmdbId, VideoDetail& videoDetail);

    bool GetTVCredits(int tmdbId, VideoDetail& videoDetail);

    bool DownloadImages(VideoInfo& videoInfo);

    bool GetMovieImages(VideoInfo& videoInfo);

    bool GetSeasonImages(VideoInfo& videoInfo, int seasonNumber);

    bool GetTVImages(VideoInfo& videoInfo, int seasonNumber);

private:

    ErrCode m_lastErrCode = NO_ERR;
};
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

    virtual bool ScrapeMovie(VideoInfo& videoInfo, int movieID);

    virtual bool ScrapeTV(VideoInfo& videoInfo, int tvId, int seasonId);

    bool UpdateTV(VideoInfo& videoInfo);

    virtual int GetLastErrCode();

    virtual const std::string& GetLastErrStr();

private:

    bool SendRequest(std::ostream& out, Poco::URI& uri);

    bool GetMovieDetail(std::ostream& out, int tmdbid);

    bool GetTVDetail(std::ostream& out, int tmdbid);

    bool GetSeasonDetail(std::ostream& out, int tmdbId, int seasonNum);

    bool GetMovieCredits(std::ostream& out, int tmdbId);

    bool GetTVCredits(std::ostream& out, int tmdbId);

    bool DownloadPoster(VideoInfo& videoInfo);

private:

    ErrCode m_lastErrCode = NO_ERR;
};
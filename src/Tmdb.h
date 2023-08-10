#pragma once

#include <iostream>

#include "CommonType.h"

bool Search(std::ostream& out, VideoType VideoType, const std::string& keywords, const std::string& year = "");

bool GetMovieDetail(std::ostream& out, int tmdbid);

bool GetTvDetail(std::ostream& out, int tmdbid);

bool GetSeasonDetail(std::ostream& out, int tmdbId, int seasonNum);

bool GetMovieCredits(std::ostream& out, int tmdbId);
bool GetTVCredits(std::ostream& out, int tmdbId);

bool DownloadPoster(VideoInfo& videoInfo);

bool UpdateTV(VideoInfo& videoInfo);

bool ScrapeMovie(VideoInfo& videoInfo, std::ostream& out);
bool ScrapeTV(VideoInfo& videoInfo, std::ostream& out, int seasonId);
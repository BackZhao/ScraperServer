#pragma once

#include "CommonType.h"

#include <iostream>

bool Search(std::ostream& out, VideoType VideoType, const std::string& keywords, const std::string& year = "");

bool GetTvDetail(std::ostream& out, int tmdbid);

bool GetSeasonDetail(std::ostream& out, int tmdbId, int seasonNum);

bool UpdateTV(VideoInfo& videoInfo);
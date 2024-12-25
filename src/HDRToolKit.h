#pragma once

#include <map>
#include <string>

/**
 * @brief 视频流的HDR类型
 * 
 */
enum class VideoRangeType {
    UNKNOWN,         // Unknown video range type.
    SDR,             // SDR video range type (8bit).
    HDR10,           // HDR10 video range type (10bit).
    HLG,             // HLG video range type (10bit).
    DOVI,            // Dolby Vision video range type (10bit encoded / 12bit remapped).
    DOVI_WITH_HDR10, // Dolby Vision with HDR10 video range fallback (10bit).
    DOVI_WITH_HLG,   // Dolby Vision with HLG video range fallback (10bit).
    DOVI_WITH_SDR,   // Dolby Vision with SDR video range fallback (8bit / 10bit).
    HDR10PLUS        // HDR10+ video range type (10bit to 16bit).
};

const std::map<VideoRangeType, std::string> VIDEO_RANGE_TYPE_TO_STR_MAP = {
    {VideoRangeType::UNKNOWN, "Unknown"},
    {VideoRangeType::SDR, "SDR"},
    {VideoRangeType::HDR10, "HDR10"},
    {VideoRangeType::HLG, "HLG"},
    {VideoRangeType::DOVI, "Dolby Vision"},
    {VideoRangeType::DOVI_WITH_HDR10, "Dolby Vision with HDR10"},
    {VideoRangeType::DOVI_WITH_HLG, "Dolby Vision with HLG"},
    {VideoRangeType::DOVI_WITH_SDR, "Dolby Vision with SDR"},
    {VideoRangeType::HDR10PLUS, "HDR10 Plus"},
};

class HDRToolKit
{
public:

    static bool Checkffprobe();

    static VideoRangeType GetHDRTypeFromFile(const std::string& fileName);

private:

    static VideoRangeType GetHDRTypeByDolbyConf(int dv_profile, int dv_bl_signal_compatibility_id);
};
#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaRtpCommonEpochPolicy : std::uint8_t {
    EarliestLockedSenderReportSourceTime = 0,
    Unknown = 255
};

const char* mediaRtpCommonEpochPolicyOptionValue(
    MediaRtpCommonEpochPolicy policy) noexcept;
::media::Result<MediaRtpCommonEpochPolicy> parseMediaRtpCommonEpochPolicy(
    std::string_view value);

} // namespace media::ffmpeg::graph

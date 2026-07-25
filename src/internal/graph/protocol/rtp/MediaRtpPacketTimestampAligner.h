#pragma once

#include "internal/graph/protocol/rtp/MediaRtpSourceClockMapper.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtpPacketTimestampAligner final {
public:
    ::media::Result<std::uint64_t> align(
        const MediaRtpSourceClockCalibration& calibration,
        std::uint32_t rawRtpTimestamp) const;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtpPacketClockProjector final {
public:
    ::media::Result<MediaPacketSourceTiming> project(
        const MediaRtpClockGroupSnapshot& snapshot,
        MediaScheduledStream stream,
        std::uint64_t extendedRtpTimestamp) const;
};

} // namespace media::ffmpeg::graph

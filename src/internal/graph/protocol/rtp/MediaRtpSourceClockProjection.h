#pragma once

#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"
#include "internal/graph/model/MediaPacketSourceTiming.h"

namespace media::ffmpeg::graph {

struct MediaRtpSourceClockProjection final {
    MediaSourceClockReadiness readiness;
    std::uint64_t generation;
    bool discontinuity;
    std::uint64_t evidenceRevision;

    bool operator==(const MediaRtpSourceClockProjection&) const noexcept = default;
};

::media::Result<MediaRtpSourceClockProjection> projectMediaRtpSourceClockState(
    const MediaRtpClockGroupSnapshot& snapshot, MediaTranscodeStreamSet members);

} // namespace media::ffmpeg::graph

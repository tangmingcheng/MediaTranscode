#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"

namespace media::ffmpeg::graph {
struct MediaRtpConfigurationStoragePlan final {
    std::size_t retainedBytes;
    std::size_t peakBytes;
};
class MediaRtpConfigurationStoragePlanner final {
public:
    // Inputs have passed the production protocol validator. This computes
    // allocation geometry only; it does not parse a second semantic protocol.
    static ::media::Result<MediaRtpConfigurationStoragePlan> plan(
        const MediaRtpDepacketizerConfig& config, const MediaPreparedRtpAccessUnitEnvelope& envelope);
};
} // namespace media::ffmpeg::graph

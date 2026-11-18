#pragma once
#include "media_transcode/Result.h"
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaEncoderSurfaceRetentionFacts {
    std::uint64_t graphInFlightSurfaces;
    std::uint64_t pipelinePendingSurfaces;
    std::uint64_t encoderRetainedSurfaces;
    std::uint64_t persistentPoolSurfaces;
    std::string authority;
    friend bool operator==(const MediaEncoderSurfaceRetentionFacts&,
                           const MediaEncoderSurfaceRetentionFacts&) = default;
};

struct MediaEncoderHardwareFramesPoolPlan final : MediaEncoderSurfaceRetentionFacts {
    std::uint64_t initialPoolSurfaces;
    friend bool operator==(const MediaEncoderHardwareFramesPoolPlan&,
                           const MediaEncoderHardwareFramesPoolPlan&) = default;
};

// Counts only. Neither a device byte bound nor authorization to allocate.
class MediaEncoderHardwareFramesPoolPlanner final {
public:
    static ::media::Result<MediaEncoderHardwareFramesPoolPlan> plan(MediaEncoderSurfaceRetentionFacts facts);
};

} // namespace media::ffmpeg::graph

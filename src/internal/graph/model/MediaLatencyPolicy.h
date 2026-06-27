#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaLatencyMode {
    Unknown,
    Throughput,
    Balanced,
    LowLatency,
    Realtime
};

struct MediaLatencyPolicy {
    MediaLatencyMode mode = MediaLatencyMode::Balanced;

    int64_t targetLatencyUs = 0;
    int64_t maxLatencyUs = 0;
    int64_t maxJitterUs = 0;

    bool enablePacing = false;
    bool dropLateFrames = false;
    bool preferKeyFrameRecovery = true;

    constexpr bool realtime() const noexcept
    {
        return mode == MediaLatencyMode::Realtime || mode == MediaLatencyMode::LowLatency;
    }
};

} // namespace media::ffmpeg::graph

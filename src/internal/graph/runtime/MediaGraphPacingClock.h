#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaTimeDescriptor.h"
#include "media_transcode/Result.h"

#include <chrono>

namespace media::ffmpeg::graph {

class MediaGraphPacingClock final {
public:
    using Clock = std::chrono::steady_clock;

    void reset();
    void setPolicy(MediaLatencyPolicy policy) noexcept;
    const MediaLatencyPolicy& policy() const noexcept;

    ::media::Status waitUntil(MediaTimeValue pts, MediaRational timeBase);

private:
    static std::chrono::microseconds toMicroseconds(MediaTimeValue pts, MediaRational timeBase) noexcept;

private:
    MediaLatencyPolicy m_policy;
    Clock::time_point m_start = Clock::time_point{};
};

} // namespace media::ffmpeg::graph

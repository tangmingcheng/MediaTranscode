#pragma once

#include "internal/graph/time/MediaMasterClock.h"

#include <chrono>

namespace media::ffmpeg::graph {

class MediaSteadyMasterClock final : public MediaMasterClock {
public:
    explicit MediaSteadyMasterClock(MediaRunningTime masterAtAnchor) noexcept;

    ::media::Result<MediaRunningTime> now() const noexcept override;

private:
    MediaRunningTime m_masterAtAnchor;
    std::chrono::steady_clock::time_point m_steadyAnchor;
};

} // namespace media::ffmpeg::graph

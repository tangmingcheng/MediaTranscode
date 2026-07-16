#pragma once

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaSourceClockStateBuffer final : public MediaBuffer {
public:
    MediaSourceClockStateBuffer(MediaSourceClockReadiness readiness,
                                std::uint64_t generation,
                                bool discontinuity);

    MediaBufferType type() const noexcept override;
    MediaSourceClockReadiness readiness() const noexcept;
    std::uint64_t generation() const noexcept;

private:
    const MediaSourceClockReadiness m_readiness;
    const std::uint64_t m_generation;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaSourceClockStateBuffer final : public MediaBuffer {
public:
    MediaSourceClockStateBuffer(MediaSourceClockReadiness readiness,
                                std::uint64_t generation,
                                bool discontinuity,
                                std::optional<std::uint64_t> evidenceRevision);

    MediaBufferType type() const noexcept override;
    MediaSourceClockReadiness readiness() const noexcept;
    std::uint64_t generation() const noexcept;
    const std::optional<std::uint64_t>& evidenceRevision() const noexcept { return m_evidenceRevision; }

private:
    const MediaSourceClockReadiness m_readiness;
    const std::uint64_t m_generation;
    const std::optional<std::uint64_t> m_evidenceRevision;
};

} // namespace media::ffmpeg::graph

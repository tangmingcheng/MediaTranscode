#pragma once

#include "internal/graph/model/MediaGraphTypes.h"

#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaMemoryBudget {
    MediaByteSize maxBytes = 0;
    MediaByteSize softLimitBytes = 0;
    MediaByteSize reservedBytes = 0;

    std::size_t maxBuffers = 0;
    std::size_t preallocatedBuffers = 0;

    bool enforceHardLimit = false;
    bool allowDynamicGrowth = true;

    constexpr bool operator==(const MediaMemoryBudget&) const noexcept = default;

    constexpr bool hasByteLimit() const noexcept
    {
        return maxBytes > 0;
    }

    constexpr bool hasBufferLimit() const noexcept
    {
        return maxBuffers > 0;
    }
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/model/MediaBackpressurePolicy.h"
#include "internal/graph/model/MediaDropPolicy.h"

#include <cstddef>

namespace media::ffmpeg::graph {

enum class MediaQueueMode {
    Unknown,
    Direct,
    Blocking,
    SpscRing,
    MpscRing
};

enum class MediaQueueOverflowPolicy {
    BlockProducer,
    DropNewest,
    DropOldest,
    DropNonKeyFrame,
    Abort
};

enum class MediaQueueOrderingPolicy {
    Fifo,
    Timestamp,
    Priority
};

struct MediaQueuePolicy {
    MediaQueueMode mode = MediaQueueMode::Blocking;
    MediaQueueOverflowPolicy overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    MediaQueueOrderingPolicy orderingPolicy = MediaQueueOrderingPolicy::Fifo;

    std::size_t capacity = 16;
    std::size_t reserveCapacity = 0;

    MediaDropPolicy dropPolicy;
    MediaBackpressurePolicy backpressurePolicy;

    bool bounded = true;
    bool preserveOrdering = true;
    bool allowFlushControlBypass = true;
    bool collectMetrics = true;

    constexpr bool operator==(const MediaQueuePolicy&) const noexcept = default;

    constexpr bool isDirect() const noexcept
    {
        return mode == MediaQueueMode::Direct;
    }

    constexpr bool isBoundedQueue() const noexcept
    {
        return bounded && capacity > 0;
    }
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/model/MediaGraphTypes.h"

namespace media::ffmpeg::graph {

struct MediaTimestampRange {
    MediaTimeValue start = invalidMediaTimeValue;
    MediaTimeValue end = invalidMediaTimeValue;

    constexpr bool hasStart() const noexcept
    {
        return start != invalidMediaTimeValue;
    }

    constexpr bool hasEnd() const noexcept
    {
        return end != invalidMediaTimeValue;
    }

    constexpr bool isBounded() const noexcept
    {
        return hasStart() && hasEnd() && end >= start;
    }
};

struct MediaTimeDescriptor {
    MediaRational timeBase;
    MediaRational frameRate;
    MediaTimestampRange range;

    MediaTimeValue startTime = invalidMediaTimeValue;
    MediaDuration duration = 0;
    MediaTimeValue ptsOffset = 0;

    bool monotonicPtsRequired = true;
    bool allowNegativeStartTime = false;
    bool realtimeClockDriven = false;

    constexpr bool hasKnownTimeBase() const noexcept
    {
        return timeBase.isKnown();
    }

    constexpr bool hasKnownFrameRate() const noexcept
    {
        return frameRate.isKnown();
    }
};

} // namespace media::ffmpeg::graph

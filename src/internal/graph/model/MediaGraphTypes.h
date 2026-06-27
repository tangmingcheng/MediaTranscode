#pragma once

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

using MediaStreamIndex = int;
using MediaTimeValue = int64_t;
using MediaByteSize = uint64_t;
using MediaDuration = int64_t;
using MediaBitrate = int64_t;
using MediaSampleRate = int;
using MediaChannelCount = int;
using MediaFrameRateNum = int;
using MediaFrameRateDen = int;

constexpr MediaStreamIndex invalidMediaStreamIndex = -1;
constexpr MediaTimeValue invalidMediaTimeValue = static_cast<MediaTimeValue>(INT64_MIN);

struct MediaRational {
    int num = 0;
    int den = 1;

    constexpr bool isValid() const noexcept
    {
        return den != 0;
    }

    constexpr bool isKnown() const noexcept
    {
        return num != 0 && den != 0;
    }
};

struct MediaSize {
    int width = 0;
    int height = 0;

    constexpr bool isValid() const noexcept
    {
        return width > 0 && height > 0;
    }
};

enum class MediaProcessingMode {
    Unknown,
    LocalFile,
    RealtimeRtp
};

enum class MediaDirection {
    Unknown,
    Input,
    Output
};

enum class MediaDataAvailability {
    Unknown,
    Required,
    Optional,
    Disabled
};

} // namespace media::ffmpeg::graph

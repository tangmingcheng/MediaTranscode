#pragma once
#include <optional>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg::graph {

struct MediaVideoEncoderColorInput final {
    AVColorRange colorRange;
    AVPixelFormat pixelFormat;
    AVPixelFormat surfacePixelFormat;
    friend bool operator==(const MediaVideoEncoderColorInput&,
                           const MediaVideoEncoderColorInput&) = default;
};

// Obtained from a complete SPS, never from an unspecified codec-context value.
struct MediaVideoColorRangeFact final {
    AVColorRange range;
    bool signaled;
    std::optional<MediaVideoEncoderColorInput> encoderInput;
    friend bool operator==(const MediaVideoColorRangeFact&,
                           const MediaVideoColorRangeFact&) = default;
};

} // namespace media::ffmpeg::graph

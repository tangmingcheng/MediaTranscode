#pragma once
#include "internal/graph/model/MediaPreparedVideoRandomAccessEnvelope.h"
#include <optional>

#include "media_transcode/Result.h"
#include <cstdint>
#include <string>
#include <vector>

struct AVCodecContext;

namespace media::ffmpeg::graph {

struct MediaVideoEncoderReadbackField final {
    std::string name;
    std::int64_t value;
    friend bool operator==(const MediaVideoEncoderReadbackField&,
                           const MediaVideoEncoderReadbackField&) = default;
};

struct MediaVideoEncoderReadback final {
    std::vector<MediaVideoEncoderReadbackField> fields;
    std::vector<std::uint8_t> extraData;
    std::optional<MediaPreparedVideoRandomAccessEnvelope> randomAccess;
    // Capture only before publication, on the codec's single owning thread.
    static ::media::Result<MediaVideoEncoderReadback> capture(AVCodecContext& encoder);
    friend bool operator==(const MediaVideoEncoderReadback&,
                           const MediaVideoEncoderReadback&) = default;
};

} // namespace media::ffmpeg::graph

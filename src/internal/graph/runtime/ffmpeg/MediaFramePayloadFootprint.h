#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "media_transcode/Result.h"

#include <cstdint>

struct AVFrame;

namespace media::ffmpeg::graph {

class MediaFramePayloadFootprint final {
public:
    static ::media::Result<std::uint64_t> logicalBytes(
        const AVFrame& frame,
        MediaStreamKind streamKind);

private:
    MediaFramePayloadFootprint() = delete;
};

} // namespace media::ffmpeg::graph

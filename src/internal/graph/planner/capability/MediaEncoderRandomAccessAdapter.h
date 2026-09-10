#pragma once
#include "internal/graph/model/MediaPreparedVideoRandomAccessEnvelope.h"
#include "media_transcode/Result.h"
#include <optional>
struct AVCodecContext;
namespace media::ffmpeg::graph {
class MediaEncoderRandomAccessAdapter final {
public:
    static ::media::Result<std::optional<MediaPreparedVideoRandomAccessEnvelope>>
        readAfterOpen(AVCodecContext& encoder);
};
} // namespace media::ffmpeg::graph

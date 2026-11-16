#pragma once

#include "internal/graph/model/MediaVideoEncoderCompletionContract.h"
#include <optional>

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderCompletionAdapter final {
public:
    // Read only, before publication on the codec owner thread. Missing facts
    // leave the optimization unauthorized without changing encoder behavior.
    static std::optional<MediaVideoEncoderCompletionContract> readAfterOpen(
        const AVCodecContext& encoder);

    MediaEncoderCompletionAdapter() = delete;
};

} // namespace media::ffmpeg::graph

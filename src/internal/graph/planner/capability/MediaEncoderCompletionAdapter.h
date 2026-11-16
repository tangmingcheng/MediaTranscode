#pragma once

#include "internal/graph/model/MediaVideoEncoderCompletionContract.h"
#include <optional>
#include "media_transcode/Result.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderCompletionAdapter final {
public:
    // Read only, before publication on the codec owner thread. Missing facts
    // leave the optimization unauthorized without changing encoder behavior.
    static std::optional<MediaVideoEncoderCompletionContract> readAfterOpen(
        const AVCodecContext& encoder);
    // Validation only: never selects or revokes a planner-authorized strategy.
    static ::media::Status validateUnchanged(const AVCodecContext& encoder,
        const MediaVideoEncoderCompletionContract& prepared);

    MediaEncoderCompletionAdapter() = delete;
};

} // namespace media::ffmpeg::graph

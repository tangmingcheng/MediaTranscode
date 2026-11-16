#pragma once
#include "internal/graph/model/MediaVideoEncoderCompletionContract.h"
#include <optional>

struct AVCodecContext;

namespace media::ffmpeg::graph {
class MediaRkmppEncoderCompletionAdapter final {
public:
    static std::optional<MediaRkmppCompletionConfiguration> configuration(const AVCodecContext& context);
    static std::optional<MediaVideoEncoderCompletionContract> readAfterOpen(
        const AVCodecContext& context, const std::array<std::int64_t, 22>& publicFields,
        std::string_view& reason);
    MediaRkmppEncoderCompletionAdapter() = delete;
};
}

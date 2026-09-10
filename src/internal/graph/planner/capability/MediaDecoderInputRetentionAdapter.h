#pragma once

#include "internal/graph/model/MediaDecoderInputRetention.h"
#include <optional>
#include <string_view>

struct AVCodecContext;

namespace media::ffmpeg::graph {
class MediaDecoderInputRetentionAdapter final {
public:
    static std::optional<MediaDecoderInputRetention> readAfterOpen(
        const AVCodecContext& context, std::string_view hardwareBackend);
};
} // namespace media::ffmpeg::graph

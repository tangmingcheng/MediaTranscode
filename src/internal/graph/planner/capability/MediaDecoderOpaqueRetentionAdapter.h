#pragma once
#include "internal/graph/model/MediaDecoderOpaqueRetention.h"
#include <optional>
#include <string_view>
struct AVCodecContext;
namespace media::ffmpeg::graph {
class MediaDecoderOpaqueRetentionAdapter final {
public:
    static std::optional<MediaDecoderOpaqueRetention> readAfterOpen(
        const AVCodecContext& context, std::string_view hardwareBackend);
};
}

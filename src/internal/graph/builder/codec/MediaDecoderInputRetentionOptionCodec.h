#pragma once
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaDecoderInputRetention.h"
#include "media_transcode/Result.h"
struct AVCodecContext;
namespace media::ffmpeg::graph {
class MediaDecoderInputRetentionOptionCodec final {
public:
    static ::media::Result<MediaNodeOptions> encode(const MediaDecoderInputRetention& retention);
    static ::media::Status validateOpened(const AVCodecContext& context, const MediaNodeOptions& options);
};
} // namespace media::ffmpeg::graph

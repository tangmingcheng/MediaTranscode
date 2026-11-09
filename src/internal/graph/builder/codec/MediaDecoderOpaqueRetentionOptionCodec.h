#pragma once
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaDecoderOpaqueRetention.h"
#include "media_transcode/Result.h"
struct AVCodecContext;
namespace media::ffmpeg::graph {
class MediaDecoderOpaqueRetentionOptionCodec final {
public:
    static ::media::Result<MediaNodeOptions> encode(const MediaDecoderOpaqueRetention& facts);
    static ::media::Status validateOpened(const AVCodecContext& context, const MediaNodeOptions& options);
};
}

#pragma once
#include "internal/graph/model/MediaDecoderOpaqueRetention.h"
#include "media_transcode/Result.h"
namespace media::ffmpeg::graph {
class MediaDecoderOpaqueRetentionPlanner final {
public:
    static ::media::Result<std::uint64_t> registryCapacity(const MediaDecoderOpaqueRetention& facts);
};
}

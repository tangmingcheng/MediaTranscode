#pragma once
#include "internal/graph/model/MediaVideoFrameReadinessPlan.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"
#include <optional>
namespace media::ffmpeg::graph {
// Only process-local prepared handles; never a public or persisted configuration.
class MediaVideoFrameReadinessPlanCodec final {
public:
    static ::media::Result<MediaNodeOptions> encode(const MediaVideoFrameProducerFacts& facts);
    static ::media::Result<std::optional<MediaVideoFrameProducerFacts>> decode(const MediaNodeOptions& options);
};
}

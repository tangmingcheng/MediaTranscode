#pragma once

#include "internal/graph/sync/MediaCanonicalAccessUnitIdentity.h"
#include "internal/graph/time/MediaMappedTimestamp.h"

namespace media::ffmpeg::graph {

struct MediaCanonicalSourceStamp final {
    MediaSourceAccessUnitIdentity identity;
    std::uint64_t generation;
    MediaTimeMappingConfidence mappingConfidence;
    MediaRunningTime presentation;
    MediaRunningTime duration;
};

} // namespace media::ffmpeg::graph

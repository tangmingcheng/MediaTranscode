#pragma once

#include "internal/graph/sync/MediaCanonicalAccessUnitIdentity.h"
#include "internal/graph/time/MediaMappedTimestamp.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/time/MediaSourceClockProjection.h"

namespace media::ffmpeg::graph {

// Identity of an input AU in decode submission order, before the decoder has
// established which pictures are displayed. Unknown timing is absent, not zero.
// This product cannot be used as a canonical presentation interval or dispatch
// contract. It deliberately has no conversion to MediaCanonicalLineage.
struct MediaVideoDecodeLineage final {
    MediaSourceAccessUnitIdentity identity;
    MediaRunningTime presentation;
    std::optional<MediaRunningTime> decode;
    std::optional<MediaRunningTime> duration;
    MediaTimeMappingConfidence mappingConfidence;
    std::uint64_t generation;
    MediaTimeValue packetPresentation;
    MediaRational packetTimeBase;
    MediaSourceClockProjection projection;
};

} // namespace media::ffmpeg::graph

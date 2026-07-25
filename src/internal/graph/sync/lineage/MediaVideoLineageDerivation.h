#pragma once

#include "internal/graph/model/MediaTimeDescriptor.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

#include <cstdint>
#include <memory>

namespace media::ffmpeg::graph {

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
deriveMediaVideoLineage(
    const MediaCanonicalLineage& source,
    std::int64_t sourcePts,
    std::int64_t outputPts,
    std::int64_t outputDuration,
    MediaRational timeBase);

} // namespace media::ffmpeg::graph

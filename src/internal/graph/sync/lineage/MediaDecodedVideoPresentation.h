#pragma once

#include "internal/graph/sync/lineage/MediaVideoDecodeLineage.h"

namespace media::ffmpeg::graph {

// Preparation and runtime use the AU's original calibrated clock mapping.
// A display point supplies no source interval or duration.
inline ::media::Result<MediaRunningTime> projectMediaDecodedVideoPresentation(
    const MediaVideoDecodeLineage& lineage, MediaTimeValue pts, MediaRational timeBase)
{
    if (pts == invalidMediaTimeValue || timeBase.num <= 0 || timeBase.den <= 0 ||
        static_cast<std::int64_t>(timeBase.num) * lineage.packetTimeBase.den !=
        static_cast<std::int64_t>(lineage.packetTimeBase.num) * timeBase.den)
        return ::media::Result<MediaRunningTime>::failure(::media::ErrorInfo::invalidArgument(
            "Decoded display point requires the AU timestamp time base"));
    return lineage.projection.project(pts);
}

} // namespace media::ffmpeg::graph

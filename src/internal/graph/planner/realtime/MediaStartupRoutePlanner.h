#pragma once
#include "internal/graph/model/MediaStartupRoutePlan.h"

namespace media::ffmpeg::graph {
class MediaStartupRoutePlanner final {
public:
    static ::media::Result<MediaStartupRouteShape> describe(MediaTranscodeStreamSet members,
        MediaVideoStartupReadiness readiness);
    static ::media::Result<MediaStartupRoutePlan> bind(MediaStartupRouteShape shape,
        const MediaEdgePolicy& metadata, const MediaEdgePolicy& atomicMetadata);
};
} // namespace media::ffmpeg::graph

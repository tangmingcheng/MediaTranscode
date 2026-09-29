#pragma once
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/MediaPreparedEncoderEmissionEnvelope.h"
#include "internal/graph/planner/realtime/MediaEncoderHardwareFramesPoolPlanner.h"
#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaVideoCanvasRetentionPlan final {
    MediaEncoderHardwareFramesPoolPlan encoderPool;
    std::size_t writableSurfaces;
    // Independent published leases; AVFrame aliases share these leases.
    std::size_t maximumPublishedLeases;
    friend bool operator==(const MediaVideoCanvasRetentionPlan&,
                           const MediaVideoCanvasRetentionPlan&) = default;
};

class MediaVideoCanvasRetentionPlanner final {
public:
    static ::media::Result<MediaVideoCanvasRetentionPlan> plan(
        const MediaGraph& graph, MediaNodeId aggregate,
        const MediaPreparedEncoderEmissionEnvelope& encoder);
};

} // namespace media::ffmpeg::graph

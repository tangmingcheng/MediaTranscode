#pragma once

#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"

namespace media::ffmpeg::graph {

// The same prepared codec/protocol facts serve pre-freeze admission and final
// source planning. No graph, startup capacity or sealed owner is required.
class MediaCompositionProducerHolderPlanner final {
public:
    static ::media::Result<MediaCompositionProducerHolderFacts> plan(
        const MediaVideoSourcePlan& video, const MediaVideoSourceTiming& timing,
        const MediaPreparedRtpAccessUnitEnvelope& videoEnvelope,
        const MediaDecoderInputRetention* audioDecoder,
        const MediaPreparedRtpAccessUnitEnvelope* audioEnvelope);
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpInputStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"

namespace media::ffmpeg::graph {

struct MediaCompositionSourcePlanningProducts;
// Borrow only original owners, never members of consumed FixedProducts.
struct MediaCompositionSourcePreparedRequest final {
    const MediaRealtimeRtpTranscodeRequest& sourceRequest;
    const MediaRealtimeRawInputPlan& raw;
    const MediaVideoSourceTiming& timing;
    const MediaRealtimeDeploymentEnvelope& deployment;
    const MediaPreparedInputRetentionStream* audioRetention;
    const MediaVideoSourcePlan& video;
};
struct MediaCompositionSourcePreparedFacts final {
    MediaCompositionSourcePreparedRequest original;
    MediaPreparedRtpSourceView source;
    MediaPreparedRtpInputStorageFacts storage;
    std::variant<MediaRealtimeGraphResourceLedgerPlan, MediaAvSourceCapacityFacts> capacity;
};

class MediaRealtimeCompositionSourcePlanner final {
public:
    static ::media::Result<MediaCompositionSourcePreparedFacts> readPrepared(
        MediaCompositionSourcePreparedRequest original,
        MediaPreparedRtpSourceView prepared,
        const MediaPreparedRealtimeEmissionSet& emission);
    static ::media::Result<MediaRealtimeCompositionSourcePlan> bind(
        MediaCompositionSourcePreparedFacts&& prepared,
        MediaCompositionSourcePlanningProducts&& products);
};

} // namespace media::ffmpeg::graph

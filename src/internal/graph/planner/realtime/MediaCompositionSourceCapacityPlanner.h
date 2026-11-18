#pragma once
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"

namespace media::ffmpeg::graph {
struct MediaCompositionSourceCapacityCandidate final {
    MediaPreparedRtpSourceContainerBounds containers;
    MediaAvSourceCapacityFacts capacity;
};
class MediaCompositionSourceCapacityPlanner final {
public:
    // Quantities only: k is the common increment, not bytes or a time horizon.
    // Kernel, topology, residence and total-cost validation are still mandatory.
    static ::media::Result<MediaCompositionSourceCapacityCandidate> plan(
        const MediaVideoSourcePlan& video, const MediaRealtimeDeploymentEnvelope& deployment,
        const MediaPreparedRtpSourceSessionPlan& session,
        const MediaPreparedRtpStorageSnapshot& held, std::size_t increment);
};
}

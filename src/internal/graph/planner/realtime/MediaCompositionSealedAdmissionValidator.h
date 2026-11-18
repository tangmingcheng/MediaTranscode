#pragma once
#include "internal/graph/planner/realtime/MediaCompositionCandidateWorkspace.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlanner.h"

namespace media::ffmpeg::graph {
class MediaCompositionSealedAdmissionValidator final {
public:
    // Readbacks and ingress refer to stable sealed owners. Compare the chosen
    // products before refreshing, then independently recompute cross-source cost.
    static ::media::Status validate(MediaCompositionCandidateWorkspace& workspace,
        std::span<const MediaCompositionSourceFixedRequest> prepared,
        std::span<const MediaCompositionSourcePreparedFacts> sealed,
        std::span<const MediaCompositionSourceCapacityCandidate> selected);
};
}

#pragma once

#include "internal/graph/model/MediaRealtimeCompositionSourceTargets.h"
#include "internal/graph/model/MediaGraphPayloadProducerFact.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"

namespace media::ffmpeg::graph {

struct MediaCompositionInputAllocationFacts final {
    MediaNodeId nodeId;
    MediaPreparedInputPayloadEnvelope payload;
    MediaRtpIngressPlan ingress;
};

// Per-source allocation facts, not simultaneous retention or global admission.
// Candidate references held by the aggregate retain their source allocation.
struct MediaRealtimeCompositionSourceResources final {
    std::size_t sourceIndex;
    std::vector<MediaCompositionInputAllocationFacts> inputs;
    std::vector<MediaGraphPayloadProducerFact> producers;
};

class MediaRealtimeCompositionSourceResourcesPlanner final {
public:
    static ::media::Result<MediaRealtimeCompositionSourceResources> plan(
        const MediaGraph& graph, const MediaRealtimeCompositionSourcePlan& source,
        const MediaRealtimeCompositionSourceTargets& targets);
};

} // namespace media::ffmpeg::graph

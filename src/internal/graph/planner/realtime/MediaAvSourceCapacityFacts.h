#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/model/MediaStartupRoutePlan.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
namespace media::ffmpeg::graph {
// Capacity negotiation facts only. No allocation owner, sealed admission or
// runtime graph identity is required to evaluate the source policy/assembly.
struct MediaAvSourceCapacityFacts final {
    MediaRealtimeMediaCapacityPlan startup;
    MediaStartupRoutePlan route;
    MediaGraphQueueParameters queues;
    std::size_t videoAcquiringCapacity;
    std::optional<std::size_t> audioAcquiringCapacity;
    MediaRunningTime acquisitionWindow;
};
}

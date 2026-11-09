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
// Non-owning-policy selection: all dynamic policy declarations already belong
// to the candidate runtime. Compare those declarations independently at bind.
struct MediaAvSourceCapacitySelection final {
    MediaRealtimeMediaCapacityPlan startup;
    MediaStartupRouteShape routeShape;
    MediaGraphQueueParameters queues;
    std::size_t videoAcquiringCapacity;
    std::optional<std::size_t> audioAcquiringCapacity;
    MediaRunningTime acquisitionWindow;
    explicit MediaAvSourceCapacitySelection(const MediaAvSourceCapacityFacts& facts) noexcept
        : startup(facts.startup), routeShape(facts.route.shape), queues(facts.queues),
          videoAcquiringCapacity(facts.videoAcquiringCapacity), audioAcquiringCapacity(facts.audioAcquiringCapacity),
          acquisitionWindow(facts.acquisitionWindow) {}
    bool sameProduct(const MediaAvSourceCapacityFacts& other,
        const MediaEdgePolicy& metadata, const MediaEdgePolicy& atomicMetadata) const noexcept
    {
        return startup == other.startup && routeShape.members == other.route.shape.members &&
            routeShape.readiness == other.route.shape.readiness && metadata == other.route.metadata &&
            atomicMetadata == other.route.atomicMetadata && queues.metadata == other.queues.metadata &&
            queues.packet == other.queues.packet && queues.frame == other.queues.frame && queues.mux == other.queues.mux &&
            videoAcquiringCapacity == other.videoAcquiringCapacity && audioAcquiringCapacity == other.audioAcquiringCapacity &&
            acquisitionWindow == other.acquisitionWindow;
    }
};
}

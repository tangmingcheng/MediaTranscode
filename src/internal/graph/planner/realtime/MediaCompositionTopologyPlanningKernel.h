#pragma once
#include "internal/graph/planner/realtime/MediaCompositionGraphTopologyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"

namespace media::ffmpeg::graph {
// Borrowed only during plan(); candidate and sealed source products use the
// same declarations without constructing a graph or a storage reservation.
struct MediaCompositionTopologySourceFacts final {
    const MediaAvSyncPlan& synchronization;
    const MediaRealtimeEdgePolicySet& policies;
    const MediaCompositionSourceBranchPlan& branches;
    std::span<const MediaSourceInputConnection> inputs;
};
// Fixed output facts are owned by the candidate workspace, never borrowed
// across a freeze. Their nested storage still requires workspace admission.
struct MediaCompositionOutputTopologyDeclarations final {
private:
    const std::shared_ptr<MediaPreparationMemoryResource> m_resource;
public:
    explicit MediaCompositionOutputTopologyDeclarations(std::shared_ptr<MediaPreparationMemoryResource> resource)
        : m_resource(std::move(resource)), videoConnections(*m_resource), audioConnections(*m_resource),
          scheduler(*m_resource), protocol(*m_resource), execution(*m_resource) {}
    MediaCompositionOutputTopologyDeclarations(const MediaCompositionOutputTopologyDeclarations&) = delete;
    MediaCompositionOutputTopologyDeclarations& operator=(const MediaCompositionOutputTopologyDeclarations&) = delete;
    MediaCompositionOutputTopologyDeclarations(MediaCompositionOutputTopologyDeclarations&&) = default;
    MediaCompositionOutputTopologyDeclarations& operator=(MediaCompositionOutputTopologyDeclarations&&) = delete;
    MediaRealtimeEdgePolicySet policies;
    MediaVideoBranchShape videoShape;
    MediaPlanningCatalog<MediaVideoBranchConnection> videoConnections;
    MediaAudioBranchShape audioShape;
    MediaPlanningCatalog<MediaAudioBranchConnection> audioConnections;
    MediaOutputSegmentDeclaration scheduler;
    MediaOutputSegmentDeclaration protocol;
    MediaOutputSegmentDeclaration execution;
};
struct MediaCompositionTopologyPlanningProducts final {
private:
    // Covers all outer catalogs even after the graph is moved to its final owner.
    std::shared_ptr<MediaPreparationMemoryResource> m_resource;
public:
    explicit MediaCompositionTopologyPlanningProducts(std::shared_ptr<MediaPreparationMemoryResource> resource)
        : m_resource(std::move(resource)), aggregate(m_resource), sources(m_resource.get()), graph(m_resource), sourcePolicies(m_resource.get()), output(m_resource) {}
    MediaCompositionTopologyPlanningProducts(const MediaCompositionTopologyPlanningProducts&) = delete;
    MediaCompositionTopologyPlanningProducts& operator=(const MediaCompositionTopologyPlanningProducts&) = delete;
    MediaCompositionTopologyPlanningProducts(MediaCompositionTopologyPlanningProducts&&) = default;
    MediaCompositionTopologyPlanningProducts& operator=(MediaCompositionTopologyPlanningProducts&&) = delete;
    MediaCompositionAggregateConnections aggregate;
    std::pmr::vector<MediaCompositionSourceTopology> sources;
    MediaCompositionGraphTopology graph;
    std::pmr::vector<MediaRealtimeEdgePolicySet> sourcePolicies;
    MediaCompositionOutputTopologyDeclarations output;
};
class MediaCompositionTopologyPlanningKernel final {
public:
    // Call after source branch/input products have been refreshed. Output
    // declarations are fixed at plan(); failure invalidates all products.
    static ::media::Status rebind(
        std::span<const MediaCompositionTopologySourceFacts> sources,
        const MediaAvContinuousAggregateTopology& aggregate,
        MediaCompositionTopologyPlanningProducts& products);
    static ::media::Result<MediaCompositionTopologyPlanningProducts> plan(
        std::span<const MediaCompositionTopologySourceFacts> sources,
        const MediaAvContinuousAggregateTopology& aggregate,
        const MediaRealtimeAvOutputRuntimePlan& output,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
};
}

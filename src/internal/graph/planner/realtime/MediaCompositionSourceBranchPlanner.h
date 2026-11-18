#pragma once
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/planner/video/MediaVideoBranchConnectionsPlanner.h"
#include "internal/graph/planner/audio/MediaAudioBranchConnectionsPlanner.h"
#include "internal/graph/model/MediaVideoSourceTiming.h"

namespace media::ffmpeg::graph {
struct MediaCompositionSourceBranchPlan final {
private:
    const std::shared_ptr<MediaPreparationMemoryResource> m_resource;
public:
    explicit MediaCompositionSourceBranchPlan(std::shared_ptr<MediaPreparationMemoryResource> resource)
        : m_resource(std::move(resource)), videoDecoder(*m_resource), videoFrames(*m_resource), audio(*m_resource) {}
    MediaCompositionSourceBranchPlan(const MediaCompositionSourceBranchPlan&) = delete;
    MediaCompositionSourceBranchPlan& operator=(const MediaCompositionSourceBranchPlan&) = delete;
    MediaCompositionSourceBranchPlan(MediaCompositionSourceBranchPlan&&) = default;
    MediaCompositionSourceBranchPlan& operator=(MediaCompositionSourceBranchPlan&&) = delete;
    const std::shared_ptr<MediaPreparationMemoryResource>& resource() const noexcept { return m_resource; }
    MediaVideoBranchShape videoDecoderShape;
    MediaVideoBranchShape videoFrameShape;
    std::optional<MediaAudioBranchShape> audioShape;
    MediaPlanningCatalog<MediaVideoBranchConnection> videoDecoder;
    MediaPlanningCatalog<MediaVideoBranchConnection> videoFrames;
    MediaPlanningCatalog<MediaAudioBranchConnection> audio;
};
class MediaCompositionSourceBranchPlanner final {
public:
    // Fixed shape and connection identity; failure invalidates the candidate.
    // Successful rebinding updates policies without allocating.
    static ::media::Status rebind(
        const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
        const MediaRealtimeEdgePolicySet& policies, MediaCompositionSourceBranchPlan& result);
    // Depends only on source timing/filter/member facts and candidate policies;
    // no sealed admission, graph nodes, or hardware allocations are required.
    // Catalog allocations are charged to the explicit preparation resource.
    static ::media::Result<MediaCompositionSourceBranchPlan> plan(
        const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
        const MediaRealtimeEdgePolicySet& policies,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
};
} // namespace media::ffmpeg::graph

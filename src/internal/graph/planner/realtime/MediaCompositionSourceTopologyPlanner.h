#pragma once
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceBranchPlanner.h"
#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionAggregateConnectionsPlanner.h"
#include "internal/graph/model/MediaRtpClockRoute.h"
#include <span>
#include <optional>
#include <string>
#include <vector>
namespace media::ffmpeg::graph {
// A synchronous, non-owning view of planning products. No sealed source
// admission or allocated graph identities are needed to project a candidate.
struct MediaCompositionSourceTopologyFacts final {
    MediaTranscodeStreamSet members;
    MediaVideoStartupReadiness readiness;
    MediaAvSyncSourceClockMode clock;
    MediaRtpControlDelivery controlDelivery;
    const MediaRealtimeEdgePolicySet& policies;
    const MediaCompositionSourceBranchPlan& branches;
    std::span<const MediaSourceInputConnection> inputs;
    std::span<const MediaCompositionAggregateConnection> aggregate;
};
enum class MediaSourceTopologyDomain { Input, Startup, Rtp, VideoDecoder, VideoFrames, Audio, Aggregate };
struct MediaSourceTopologyRole final {
    MediaSourceTopologyDomain domain;
    std::size_t role;
    bool operator==(const MediaSourceTopologyRole&) const = default;
};
struct MediaSourceTopologyBinding final {
    MediaSourceTopologyRole role;
    std::size_t position;
};
struct MediaSourceTopologyConnection final {
    MediaSourceTopologyRole from;
    MediaSourceTopologyRole to;
    std::pmr::string output;
    std::pmr::string input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
};
struct MediaCompositionSourceTopology final {
private:
    // Destroyed after the catalogs, including empty containers retaining an allocator.
    std::shared_ptr<MediaPreparationMemoryResource> m_resource;
public:
    explicit MediaCompositionSourceTopology(std::shared_ptr<MediaPreparationMemoryResource> resource)
        : m_resource(std::move(resource)), positions(m_resource.get()), incidentCapacities(m_resource.get()), bindings(m_resource.get()), connections(m_resource.get()) {}
    MediaCompositionSourceTopology(const MediaCompositionSourceTopology&) = delete;
    MediaCompositionSourceTopology& operator=(const MediaCompositionSourceTopology&) = delete;
    MediaCompositionSourceTopology(MediaCompositionSourceTopology&&) noexcept = default;
    // PMR assignment does not propagate allocators. Replace the complete product
    // so moving across owners never leaves containers on the retired resource.
    MediaCompositionSourceTopology& operator=(MediaCompositionSourceTopology&& other) noexcept {
        if (this != &other) {
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
        }
        return *this;
    }
    const std::shared_ptr<MediaPreparationMemoryResource>& resource() const noexcept { return m_resource; }
    // A kind declares a source-owned node. Empty kind is an external boundary,
    // whose other incident edges and retention are deliberately not projected.
    std::pmr::vector<std::optional<MediaNodeKind>> positions;
    // Complete only for owned positions; external positions deliberately have no bound.
    std::pmr::vector<std::optional<std::pmr::vector<std::uint64_t>>> incidentCapacities;
    std::pmr::vector<MediaSourceTopologyBinding> bindings;
    std::pmr::vector<MediaSourceTopologyConnection> connections;
};
class MediaCompositionSourceTopologyPlanner final {
public:
    // Same fixed topology; success allocates nothing. Discard candidate on failure.
    static ::media::Status rebind(const MediaCompositionSourceTopologyFacts& facts,
        MediaCompositionSourceTopology& topology);
    static ::media::Result<MediaCompositionSourceTopology> plan(
        const MediaCompositionSourceTopologyFacts& facts,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
};
}

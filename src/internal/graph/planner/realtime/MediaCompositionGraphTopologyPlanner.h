#pragma once
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceTopologyPlanner.h"
#include "internal/graph/planner/realtime/MediaOutputSegmentPlanner.h"
namespace media::ffmpeg::graph {
enum class MediaCompositionGraphDomain { Source, VideoOutput, AudioOutput, Aggregate, Scheduler, Protocol, Execution };
struct MediaCompositionGraphRole final {
    MediaCompositionGraphDomain domain;
    std::optional<std::size_t> source;
    std::size_t segment;
    std::size_t role;
    bool operator==(const MediaCompositionGraphRole&) const = default;
};
inline MediaCompositionGraphRole compositionSourceRole(std::size_t source, MediaSourceTopologyRole role) {
    return {MediaCompositionGraphDomain::Source, source, static_cast<std::size_t>(role.domain), role.role};
}
template<class Role>
MediaCompositionGraphRole compositionOutputRole(MediaCompositionGraphDomain domain, Role role) {
    return {domain, std::nullopt, 0, static_cast<std::size_t>(role)};
}
struct MediaCompositionGraphAttachment final {
    MediaCompositionGraphRole role;
    MediaCompositionGraphRole owner;
    std::pmr::string port;
};
struct MediaCompositionGraphBinding final { MediaCompositionGraphRole role; std::size_t position; };
struct MediaCompositionGraphPosition final {
    MediaNodeKind kind;
    std::optional<std::size_t> source;
    std::pmr::vector<std::uint64_t> incidentCapacities;
};
struct MediaCompositionGraphConnection final {
    MediaCompositionGraphRole from;
    MediaCompositionGraphRole to;
    std::pmr::string output;
    std::pmr::string input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
};
struct MediaCompositionGraphTopology final {
private:
    // Destroyed after the catalogs, including empty containers retaining an allocator.
    std::shared_ptr<MediaPreparationMemoryResource> m_resource;
public:
    explicit MediaCompositionGraphTopology(std::shared_ptr<MediaPreparationMemoryResource> resource)
        : m_resource(std::move(resource)), positions(m_resource.get()), bindings(m_resource.get()), attachments(m_resource.get()), connections(m_resource.get()) {}
    MediaCompositionGraphTopology(const MediaCompositionGraphTopology&) = delete;
    MediaCompositionGraphTopology& operator=(const MediaCompositionGraphTopology&) = delete;
    MediaCompositionGraphTopology(MediaCompositionGraphTopology&&) noexcept = default;
    // PMR assignment does not propagate allocators. Replace the complete product
    // so moving across owners never leaves containers on the retired resource.
    MediaCompositionGraphTopology& operator=(MediaCompositionGraphTopology&& other) noexcept {
        if (this != &other) {
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
        }
        return *this;
    }
    const std::shared_ptr<MediaPreparationMemoryResource>& resource() const noexcept { return m_resource; }
    std::pmr::vector<MediaCompositionGraphPosition> positions;
    std::pmr::vector<MediaCompositionGraphBinding> bindings;
    std::pmr::vector<MediaCompositionGraphAttachment> attachments;
    std::pmr::vector<MediaCompositionGraphConnection> connections;
    std::optional<std::size_t> position(MediaCompositionGraphRole role) const noexcept;
    const MediaCompositionGraphAttachment* attachment(MediaCompositionGraphRole role) const noexcept;
};
struct MediaCompositionGraphTopologyFacts final {
    std::span<const MediaCompositionSourceTopology> sources;
    const MediaCompositionAggregateConnections& aggregate;
    const MediaVideoBranchShape& videoShape;
    std::span<const MediaVideoBranchConnection> videoConnections;
    const MediaAudioBranchShape& audioShape;
    std::span<const MediaAudioBranchConnection> audioConnections;
    const MediaOutputSegmentDeclaration& scheduler;
    const MediaOutputSegmentDeclaration& protocol;
    const MediaOutputSegmentDeclaration& execution;
};
class MediaCompositionGraphTopologyPlanner final {
public:
    // Fixed declarations and identity; successful updates allocate nothing.
    // A failed partially updated candidate must be discarded.
    static ::media::Status rebind(const MediaCompositionGraphTopologyFacts& facts,
        MediaCompositionGraphTopology& topology);
    static ::media::Result<MediaCompositionGraphTopology> plan(const MediaCompositionGraphTopologyFacts& facts,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
};
}

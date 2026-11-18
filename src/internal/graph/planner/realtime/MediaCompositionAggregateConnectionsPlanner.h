#pragma once
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "media_transcode/Result.h"
#include <span>
#include "internal/graph/model/MediaNodeKind.h"
#include <array>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {
// Endpoint roles bind actual builder results; they are not synthetic graph nodes.
enum class MediaCompositionAggregateRole {
    SourceVideoFrame, SourceAudioFrame, SourceActivated,
    VideoAggregate, AudioAggregate,
    OutputVideoCodec, OutputAudioCodec, OutputVideoFrame, OutputAudioFrame
};
struct MediaCompositionAggregateConnection final {
    MediaCompositionAggregateRole from;
    std::pmr::string output; // Empty means the actual branch endpoint's port.
    MediaCompositionAggregateRole to;
    std::pmr::string input;
    MediaStreamKind stream;
    MediaPayloadKind payload;
    MediaEdgePolicy policy;
};
struct MediaCompositionAggregateNode final { MediaCompositionAggregateRole role; MediaNodeKind kind; };
inline constexpr std::array MediaCompositionAggregateNodes{
    MediaCompositionAggregateNode{MediaCompositionAggregateRole::VideoAggregate, MediaNodeKind::AvContinuousAggregate},
    MediaCompositionAggregateNode{MediaCompositionAggregateRole::AudioAggregate, MediaNodeKind::ContinuousAudioAggregate}};
struct MediaCompositionAggregateConnections final {
private:
    std::shared_ptr<MediaPreparationMemoryResource> m_resource;
public:
    explicit MediaCompositionAggregateConnections(std::shared_ptr<MediaPreparationMemoryResource> resource)
        : m_resource(std::move(resource)), sources(m_resource.get()), output(m_resource.get()) {}
    MediaCompositionAggregateConnections(const MediaCompositionAggregateConnections&) = delete;
    MediaCompositionAggregateConnections& operator=(const MediaCompositionAggregateConnections&) = delete;
    MediaCompositionAggregateConnections(MediaCompositionAggregateConnections&&) noexcept = default;
    // Replace allocator and owner together; PMR move assignment alone does not.
    MediaCompositionAggregateConnections& operator=(MediaCompositionAggregateConnections&& other) noexcept {
        if (this != &other) {
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
        }
        return *this;
    }
    const std::shared_ptr<MediaPreparationMemoryResource>& resource() const noexcept { return m_resource; }
    // Source positions are the same positions as the aggregate source contract.
    std::pmr::vector<std::pmr::vector<MediaCompositionAggregateConnection>> sources;
    std::pmr::vector<MediaCompositionAggregateConnection> output;
};
class MediaCompositionAggregateConnectionsPlanner final {
public:
    // Fixed connection identity; discard the candidate on failure.
    static ::media::Status rebind(
        std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
        std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
        const MediaRealtimeEdgePolicySet& outputPolicies, MediaCompositionAggregateConnections& connections);
    static std::optional<MediaNodeKind> nodeKind(MediaCompositionAggregateRole role) noexcept {
        for(const auto& node:MediaCompositionAggregateNodes)if(node.role==role)return node.kind;
        return std::nullopt;
    }
    // Connection facts need no sealed storage, allocated nodes or device objects.
    static ::media::Result<MediaCompositionAggregateConnections> plan(
        std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
        std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
        const MediaRealtimeEdgePolicySet& outputPolicies,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
};
} // namespace media::ffmpeg::graph

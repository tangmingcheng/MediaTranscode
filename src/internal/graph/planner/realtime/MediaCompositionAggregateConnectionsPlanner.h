#pragma once
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
    std::string output; // Empty means the actual branch endpoint's port.
    MediaCompositionAggregateRole to;
    std::string input;
    MediaStreamKind stream;
    MediaPayloadKind payload;
    MediaEdgePolicy policy;
};
struct MediaCompositionAggregateNode final { MediaCompositionAggregateRole role; MediaNodeKind kind; };
inline constexpr std::array MediaCompositionAggregateNodes{
    MediaCompositionAggregateNode{MediaCompositionAggregateRole::VideoAggregate, MediaNodeKind::AvContinuousAggregate},
    MediaCompositionAggregateNode{MediaCompositionAggregateRole::AudioAggregate, MediaNodeKind::ContinuousAudioAggregate}};
struct MediaCompositionAggregateConnections final {
    // Source positions are the same positions as the aggregate source contract.
    std::vector<std::vector<MediaCompositionAggregateConnection>> sources;
    std::vector<MediaCompositionAggregateConnection> output;
};
class MediaCompositionAggregateConnectionsPlanner final {
public:
    static std::optional<MediaNodeKind> nodeKind(MediaCompositionAggregateRole role) noexcept {
        for(const auto& node:MediaCompositionAggregateNodes)if(node.role==role)return node.kind;
        return std::nullopt;
    }
    // Connection facts need no sealed storage, allocated nodes or device objects.
    static ::media::Result<MediaCompositionAggregateConnections> plan(
        std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
        std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
        const MediaRealtimeEdgePolicySet& outputPolicies);
};
} // namespace media::ffmpeg::graph

#pragma once
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
    std::string port;
};
struct MediaCompositionGraphBinding final { MediaCompositionGraphRole role; std::size_t position; };
struct MediaCompositionGraphPosition final {
    MediaNodeKind kind;
    std::optional<std::size_t> source;
    std::vector<std::uint64_t> incidentCapacities;
};
struct MediaCompositionGraphConnection final {
    MediaCompositionGraphRole from;
    MediaCompositionGraphRole to;
    std::string output;
    std::string input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
};
struct MediaCompositionGraphTopology final {
    std::vector<MediaCompositionGraphPosition> positions;
    std::vector<MediaCompositionGraphBinding> bindings;
    std::vector<MediaCompositionGraphAttachment> attachments;
    std::vector<MediaCompositionGraphConnection> connections;
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
    static ::media::Result<MediaCompositionGraphTopology> plan(const MediaCompositionGraphTopologyFacts& facts);
};
}

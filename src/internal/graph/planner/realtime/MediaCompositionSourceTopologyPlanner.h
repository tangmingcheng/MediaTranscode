#pragma once
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
    std::string output;
    std::string input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
};
struct MediaCompositionSourceTopology final {
    // A kind declares a source-owned node. Empty kind is an external boundary,
    // whose other incident edges and retention are deliberately not projected.
    std::vector<std::optional<MediaNodeKind>> positions;
    // Complete only for owned positions; external positions deliberately have no bound.
    std::vector<std::optional<std::vector<std::uint64_t>>> incidentCapacities;
    std::vector<MediaSourceTopologyBinding> bindings;
    std::vector<MediaSourceTopologyConnection> connections;
};
class MediaCompositionSourceTopologyPlanner final {
public:
    // Same fixed topology; success allocates nothing. Discard candidate on failure.
    static ::media::Status rebind(const MediaCompositionSourceTopologyFacts& facts,
        MediaCompositionSourceTopology& topology);
    static ::media::Result<MediaCompositionSourceTopology> plan(
        const MediaCompositionSourceTopologyFacts& facts);
};
}

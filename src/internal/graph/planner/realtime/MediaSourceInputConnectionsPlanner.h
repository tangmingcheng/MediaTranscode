#pragma once
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include "media_transcode/Result.h"
#include <string_view>
#include <vector>
#include <optional>
#include "internal/graph/model/MediaStartupRoutePlan.h"

namespace media::ffmpeg::graph {
enum class MediaSourceInputRole {
    RawVideo, RawAudio, ProtocolVideo, ProtocolAudio, ProtocolClock,
    SourceClock, VideoGate, AudioGate, VideoStartup, AudioStartup,
    StartupClock, Coordinator, DecoderFrame
};
struct MediaSourceInputNodeShape final {
    MediaStartupRouteShape startup;
    MediaAvSyncSourceClockMode clock;
    static std::optional<MediaStartupRouteRole> startupRole(MediaSourceInputRole role) noexcept;
    bool has(MediaSourceInputRole role) const noexcept;
    std::optional<MediaNodeKind> ownedKind(MediaSourceInputRole role) const noexcept;
};
enum class MediaSourceInputConnectionPhase { ProtocolPackets, Shared, DecodedFrame };
struct MediaSourceInputConnection final {
    MediaSourceInputRole from;
    std::string_view output;
    MediaSourceInputRole to;
    std::string_view input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
    std::string_view label;
    MediaSourceInputConnectionPhase phase;
};
// Clock evidence/snapshots and startup batches use their existing route
// products. This product describes the connections between those segments.
class MediaSourceInputConnectionsPlanner final {
public:
    static ::media::Result<std::vector<MediaSourceInputConnection>> plan(
        MediaTranscodeStreamSet members, MediaVideoStartupReadiness readiness,
        MediaAvSyncSourceClockMode clock, const MediaRealtimeEdgePolicySet& policies);
};
} // namespace media::ffmpeg::graph

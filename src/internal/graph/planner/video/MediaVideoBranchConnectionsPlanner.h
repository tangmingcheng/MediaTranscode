#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "media_transcode/Result.h"
#include <array>
#include <optional>
#include "internal/graph/model/MediaNodeKind.h"
#include <string_view>
#include <vector>
#include <span>

namespace media::ffmpeg::graph {

// External roles are bindings, not graph nodes. The same connection product is
// usable before node allocation and when binding the production graph.
enum class MediaVideoBranchRole : std::size_t {
    Format, Packet, Frame, OutputCodec, SharedCodec, CodecResolver, StartGate,
    Decode, SourceCopy, Fanout, Transfer, Timestamp, FrameRate, Filter,
    Preparation, Encode, Count
};
struct MediaVideoBranchShape final {
    std::array<bool, static_cast<std::size_t>(MediaVideoBranchRole::Count)> present;
    // No kind denotes an external endpoint; presence is declared independently.
    static std::optional<MediaNodeKind> ownedKind(MediaVideoBranchRole role) noexcept;
    bool has(MediaVideoBranchRole role) const noexcept { return present[static_cast<std::size_t>(role)]; }
};
struct MediaVideoBranchConnection final {
    MediaVideoBranchRole from;
    std::string_view output;
    MediaVideoBranchRole to;
    std::string_view input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
    std::string_view label;
};
struct MediaVideoBranchConnectionPolicies final {
    MediaEdgePolicy metadata;
    MediaEdgePolicy packet;
    MediaEdgePolicy frame;
    MediaEdgePolicy inputFrame;
    MediaEdgePolicy encoderFrame;
};
class MediaVideoBranchConnectionsPlanner final {
public:
    // Fixed declaration identity; only policies change. Failure invalidates
    // the candidate. Successful rebinding performs no allocations.
    static ::media::Status rebind(const MediaVideoBranchShape& shape,
        const MediaVideoBranchConnectionPolicies& policies, std::span<MediaVideoBranchConnection> connections);
    static MediaVideoBranchShape outputEncoder() noexcept;
    static MediaVideoBranchShape sourceFrames(bool canonicalCadence, bool filterActive) noexcept;
    static MediaVideoBranchShape sourceDecoder() noexcept;
    static ::media::Result<std::vector<MediaVideoBranchConnection>> plan(
        const MediaVideoBranchShape& shape, const MediaVideoBranchConnectionPolicies& policies);
};
} // namespace media::ffmpeg::graph

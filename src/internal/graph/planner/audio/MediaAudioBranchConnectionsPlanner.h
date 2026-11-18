#pragma once
#include "internal/graph/planner/MediaPlanningCatalog.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "media_transcode/Result.h"
#include <array>
#include <optional>
#include "internal/graph/model/MediaNodeKind.h"
#include <string_view>
#include <vector>
#include <span>

namespace media::ffmpeg::graph {
enum class MediaAudioBranchRole : std::size_t {
    Format, Packet, OutputCodec, Normalize, CodecResolver, Decode, Trim,
    Drift, Resample, Encode, Canonicalizer, Count
};
struct MediaAudioBranchShape final {
    std::array<bool, static_cast<std::size_t>(MediaAudioBranchRole::Count)> present;
    bool externalCorrection;
    // No kind denotes an external endpoint; presence is declared independently.
    static std::optional<MediaNodeKind> ownedKind(MediaAudioBranchRole role) noexcept;
    bool has(MediaAudioBranchRole role) const noexcept { return present[static_cast<std::size_t>(role)]; }
};
struct MediaAudioBranchConnection final {
    MediaAudioBranchRole from;
    std::string_view output;
    MediaAudioBranchRole to;
    std::string_view input;
    MediaPayloadKind payload;
    MediaStreamKind stream;
    MediaEdgePolicy policy;
    std::string_view label;
};
class MediaAudioBranchConnectionsPlanner final {
public:
    // Fixed declaration identity; only policies change. Failure invalidates
    // the candidate. Successful rebinding performs no allocations.
    static ::media::Status rebind(const MediaAudioBranchShape& shape,
        const MediaRealtimeEdgePolicySet& policies, std::span<MediaAudioBranchConnection> connections);
    static MediaAudioBranchShape outputEncoder(bool synchronized) noexcept;
    static MediaAudioBranchShape source(bool normalizePackets, bool synchronized, bool externalCorrection) noexcept;
    static ::media::Result<MediaPlanningCatalog<MediaAudioBranchConnection>> plan(
        const MediaAudioBranchShape& shape, const MediaRealtimeEdgePolicySet& policies, std::pmr::memory_resource& resource);
};
} // namespace media::ffmpeg::graph

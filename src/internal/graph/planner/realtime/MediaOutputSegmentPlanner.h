#pragma once
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "media_transcode/Result.h"
#include <optional>
#include <vector>
#include <string_view>
namespace media::ffmpeg::graph {
enum class MediaOutputSegmentRole : std::size_t {
    Activation, VideoCodec, AudioCodec, VideoPacket, AudioPacket, SerializedPacket,
    Scheduler, Router, TransportPlan, Sender, VideoMaterializer, AudioMaterializer,
    Sdp, TsPlan, TsAdapter, TsMux, TsMaterializer, Count
};
struct MediaOutputSegmentNode final { MediaOutputSegmentRole role; MediaNodeKind kind; };
struct MediaOutputSegmentConnection final {
    MediaOutputSegmentRole from;
    std::string_view output;
    MediaOutputSegmentRole to;
    std::string_view input;
    MediaStreamKind stream;
    MediaPayloadKind payload;
    MediaEdgePolicy policy;
    std::string_view label;
};
struct MediaOutputSegmentDeclaration final {
    std::vector<MediaOutputSegmentNode> nodes;
    std::vector<MediaOutputSegmentConnection> connections;
    std::optional<MediaNodeKind> kind(MediaOutputSegmentRole role) const noexcept {
        for(const auto& node:nodes)if(node.role==role)return node.kind;
        return std::nullopt;
    }
};
class MediaOutputSegmentPlanner final {
public:
    static MediaNodeKind nodeKind(MediaOutputSegmentRole role) noexcept;
    static ::media::Result<MediaOutputSegmentDeclaration> scheduler(const MediaRealtimeEdgePolicySet& policies,
        const MediaEdgePolicy& audioPolicy);
    static ::media::Result<MediaOutputSegmentDeclaration> execution(const MediaRealtimeEdgePolicySet& policies);
    static ::media::Result<MediaOutputSegmentDeclaration> rtp(bool audio, MediaPayloadKind codecPayload,
        const MediaRealtimeEdgePolicySet& policies);
    static ::media::Result<MediaOutputSegmentDeclaration> mpegTs(bool audio, bool rtp,
        MediaPayloadKind codecPayload, const MediaRealtimeEdgePolicySet& policies);
};
}

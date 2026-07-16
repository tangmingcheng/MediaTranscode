#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/builder/MediaEndpoint.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaPacketCopyBranchOptions {
    std::string prefix;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    int sourceStreamIndex = invalidMediaStreamIndex;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort;

    bool monotonicPacketTimestamps = false;
    std::optional<bool> normalizePackets;

    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
};

struct MediaPacketCopyBranchResult final {
    MediaEndpoint codec;
    MediaEndpoint packet;
};

class MediaPacketCopyBranchBuilder final {
public:
    static ::media::Result<MediaPacketCopyBranchResult> build(
        MediaGraph& graph,
        const MediaPacketCopyBranchOptions& options);

private:
    MediaPacketCopyBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph

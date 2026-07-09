#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/model/MediaStreamKind.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeEdgePolicySet {
    MediaEdgePolicy metadata;
    MediaEdgePolicy packet;
    MediaEdgePolicy videoPacket;
    MediaEdgePolicy audioPacket;
    MediaEdgePolicy frame;
    MediaEdgePolicy mux;

    const MediaEdgePolicy& packetPolicy(MediaStreamKind streamKind) const noexcept
    {
        return streamKind == MediaStreamKind::Audio ? audioPacket : videoPacket;
    }
};

} // namespace media::ffmpeg::graph

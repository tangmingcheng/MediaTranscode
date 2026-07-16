#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/model/MediaStreamKind.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeEdgePolicySet {
    MediaEdgePolicy metadata;
    MediaEdgePolicy packet;
    MediaEdgePolicy videoPacket;
    MediaEdgePolicy audioPacket;
    MediaEdgePolicy synchronizedPacket;
    MediaEdgePolicy frame;
    MediaEdgePolicy mux;
    MediaEdgePolicy videoMux;
    MediaEdgePolicy audioMux;

    const MediaEdgePolicy& packetPolicy(MediaStreamKind streamKind) const noexcept
    {
        return streamKind == MediaStreamKind::Audio ? audioPacket : videoPacket;
    }

    const MediaEdgePolicy& muxPolicy(MediaStreamKind streamKind) const noexcept
    {
        return streamKind == MediaStreamKind::Audio ? audioMux : videoMux;
    }
};

} // namespace media::ffmpeg::graph

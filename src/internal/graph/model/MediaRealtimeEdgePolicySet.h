#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/model/MediaStreamKind.h"

namespace media::ffmpeg::graph {

struct MediaVideoLineageEdgePolicySet final {
    MediaEdgePolicy ingressPacket;
    MediaEdgePolicy startupPacket;
    MediaEdgePolicy frame;
    MediaEdgePolicy preparedFrame;

    constexpr bool operator==(
        const MediaVideoLineageEdgePolicySet&) const noexcept = default;
};

struct MediaRealtimeEdgePolicySet {
    MediaEdgePolicy metadata;
    MediaEdgePolicy packet;
    MediaEdgePolicy videoPacket;
    MediaEdgePolicy audioPacket;
    MediaEdgePolicy synchronizedPacket;
    MediaEdgePolicy audioDriftTransaction;
    MediaEdgePolicy videoFrame;
    MediaEdgePolicy synchronizedVideoFrame;
    MediaEdgePolicy preparedVideoFrame;
    MediaEdgePolicy audioFrame;
    MediaEdgePolicy mux;
    MediaEdgePolicy videoMux;
    MediaEdgePolicy audioMux;
    MediaEdgePolicy atomicMetadata;
    MediaEdgePolicy atomicVideoPacket;
    MediaEdgePolicy atomicAudioPacket;

    constexpr bool operator==(
        const MediaRealtimeEdgePolicySet&) const noexcept = default;

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

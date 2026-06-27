#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/runtime/channel/MediaChannelId.h"

namespace media::ffmpeg::graph {

struct MediaChannelBinding {
    MediaChannelId channelId = MediaChannelId::invalid();
    MediaEdgeId edgeId = MediaEdgeId::invalid();

    MediaEdgeEndpoint from;
    MediaEdgeEndpoint to;

    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaEdgeKind edgeKind = MediaEdgeKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;

    bool isValid() const noexcept
    {
        return channelId.isValid() && edgeId.isValid() && from.isValid() && to.isValid();
    }
};

} // namespace media::ffmpeg::graph

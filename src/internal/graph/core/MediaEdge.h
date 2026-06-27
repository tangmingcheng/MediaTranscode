#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaEdgeEndpoint {
    MediaNodeId nodeId;
    MediaPortId portId;

    bool isValid() const
    {
        return nodeId.isValid() && portId.isValid();
    }
};

struct MediaEdge {
    MediaEdgeId id;
    std::string name;

    MediaEdgeEndpoint from;
    MediaEdgeEndpoint to;

    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaEdgeKind edgeKind = MediaEdgeKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;

    bool required = true;

    bool isValid() const
    {
        return id.isValid() && from.isValid() && to.isValid();
    }
};

} // namespace media::ffmpeg::graph

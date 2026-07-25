#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaBackpressurePolicy.h"
#include "internal/graph/model/MediaBufferPolicy.h"
#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaEdgeEndpoint {
    MediaNodeId nodeId;
    MediaPortId portId;

    bool isValid() const noexcept
    {
        return nodeId.isValid() && portId.isValid();
    }
};

struct MediaEdgePolicy {
    MediaQueuePolicy queuePolicy;
    MediaBufferPolicy bufferPolicy;
    MediaBackpressurePolicy backpressurePolicy;

    constexpr bool operator==(const MediaEdgePolicy&) const noexcept = default;
};

struct MediaEdge {
    MediaEdgeId id;
    std::string name;

    MediaEdgeEndpoint from;
    MediaEdgeEndpoint to;

    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaEdgeKind edgeKind = MediaEdgeKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;

    MediaFormatDescriptor format;
    MediaTimeDescriptor time;
    MediaHardwareDescriptor hardware;
    MediaEdgePolicy policy;

    bool required = true;

    bool isValid() const noexcept
    {
        return id.isValid() && from.isValid() && to.isValid();
    }
};

} // namespace media::ffmpeg::graph

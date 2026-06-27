#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaPortDirection {
    Unknown,
    Input,
    Output
};

struct MediaPort {
    MediaPortId id;
    MediaNodeId nodeId;

    std::string name;
    MediaPortDirection direction = MediaPortDirection::Unknown;

    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaEdgeKind edgeKind = MediaEdgeKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;

    bool required = true;
    bool multiple = false;

    bool isInput() const
    {
        return direction == MediaPortDirection::Input;
    }

    bool isOutput() const
    {
        return direction == MediaPortDirection::Output;
    }

    bool accepts(const MediaPort& upstream) const
    {
        if (!isInput() || !upstream.isOutput()) {
            return false;
        }

        const bool streamMatches =
            streamKind == MediaStreamKind::Any ||
            upstream.streamKind == MediaStreamKind::Any ||
            streamKind == upstream.streamKind;

        const bool edgeMatches =
            edgeKind == MediaEdgeKind::Unknown ||
            upstream.edgeKind == MediaEdgeKind::Unknown ||
            edgeKind == upstream.edgeKind;

        const bool payloadMatches =
            payloadKind == MediaPayloadKind::Unknown ||
            upstream.payloadKind == MediaPayloadKind::Unknown ||
            payloadKind == upstream.payloadKind;

        return streamMatches && edgeMatches && payloadMatches;
    }
};

} // namespace media::ffmpeg::graph

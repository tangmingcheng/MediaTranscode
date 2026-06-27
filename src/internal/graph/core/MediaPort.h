#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

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

    MediaFormatDescriptor format;
    MediaTimeDescriptor time;
    MediaHardwareDescriptor hardware;

    bool required = true;
    bool multiple = false;

    bool isInput() const noexcept
    {
        return direction == MediaPortDirection::Input;
    }

    bool isOutput() const noexcept
    {
        return direction == MediaPortDirection::Output;
    }

    bool hasFormatDescriptor() const noexcept
    {
        return format.streamKind != MediaStreamKind::Unknown ||
               format.hasStreamIndex() ||
               format.video.hasKnownSize() ||
               format.audio.hasKnownLayout() ||
               !format.codec.codecName.empty();
    }

    bool hasTimeDescriptor() const noexcept
    {
        return time.hasKnownTimeBase() ||
               time.hasKnownFrameRate() ||
               time.startTime != invalidMediaTimeValue ||
               time.duration > 0;
    }

    bool hasHardwareDescriptor() const noexcept
    {
        return hardware.deviceKind != MediaHardwareDeviceKind::Unknown ||
               hardware.frameKind != MediaHardwareFrameKind::Unknown ||
               !hardware.deviceName.empty() ||
               !hardware.pixelFormat.empty();
    }

    bool accepts(const MediaPort& upstream) const noexcept
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

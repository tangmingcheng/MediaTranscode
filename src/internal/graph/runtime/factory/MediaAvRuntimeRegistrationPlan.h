#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaProcessingNodeOwnership.h"

#include "internal/graph/model/MediaRtpControlDelivery.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaAvDemuxClockRegistration final {
    MediaNodeId videoBinder;
    MediaNodeId audioBinder;
};

struct MediaAvRtpClockRegistration final {
    MediaNodeId clockGroup;
    MediaNodeId videoRaw;
    MediaNodeId videoBinder;
    std::optional<MediaNodeId> audioRaw;
    std::optional<MediaNodeId> audioBinder;
    std::optional<MediaNodeId> videoControl;
    std::optional<MediaNodeId> audioControl;
    MediaRtpControlDelivery controlDelivery;
};

struct MediaAvRuntimeInputRegistration final {
    MediaNodeId epochBinder;
    MediaNodeId activationSequencer;
    MediaNodeId releaseExtractor;
    std::optional<MediaAvDemuxClockRegistration> demuxClock;
    std::optional<MediaAvRtpClockRegistration> rtpClock;
};

struct MediaAvRuntimeRegistrationPlan final {
    MediaAvRuntimeInputRegistration input;
    MediaNodeId preparationOwner;
    MediaNodeId outputScheduler;
    std::optional<MediaNodeId> rtpSdpPublisher;
    MediaProcessingNodeOwnership processing;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/core/MediaNodeId.h"

#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvDemuxClockRegistration final {
    MediaNodeId videoBinder;
    MediaNodeId audioBinder;
};

struct MediaAvRuntimeInputRegistration final {
    MediaNodeId epochBinder;
    MediaNodeId activationSequencer;
    MediaNodeId releaseExtractor;
    std::optional<MediaAvDemuxClockRegistration> demuxClock;
};

struct MediaAvRuntimeRegistrationPlan final {
    MediaAvRuntimeInputRegistration input;
    MediaNodeId preparationOwner;
    MediaNodeId outputScheduler;
    std::optional<MediaNodeId> rtpSdpPublisher;
    std::vector<MediaNodeId> members;
};

} // namespace media::ffmpeg::graph

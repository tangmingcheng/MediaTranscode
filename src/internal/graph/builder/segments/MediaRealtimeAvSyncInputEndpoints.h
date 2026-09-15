#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/runtime/factory/MediaAvRuntimeRegistrationPlan.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncInputEndpoints final {
    MediaEndpoint releasedVideo;
    MediaEndpoint releasedAudio;
    MediaEndpoint activatedRelease;
    MediaAvRuntimeInputRegistration registration;
};

} // namespace media::ffmpeg::graph

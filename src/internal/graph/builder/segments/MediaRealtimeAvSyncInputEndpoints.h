#pragma once

#include "internal/graph/builder/MediaEndpoint.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncInputEndpoints final {
    MediaEndpoint releasedVideo;
    MediaEndpoint releasedAudio;
    MediaEndpoint activatedRelease;
};

} // namespace media::ffmpeg::graph

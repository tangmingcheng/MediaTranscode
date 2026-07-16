#pragma once

#include "internal/graph/core/MediaNodeId.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaEndpoint final {
    MediaNodeId node;
    std::string port;

    bool valid() const noexcept
    {
        return node.isValid() && !port.empty();
    }
};

struct MediaRealtimeAvSyncInputEndpoints final {
    MediaEndpoint releasedVideo;
    MediaEndpoint releasedAudio;
    MediaEndpoint activatedRelease;
};

} // namespace media::ffmpeg::graph

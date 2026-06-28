#pragma once

#include "internal/graph/runtime/distributed/MediaGraphNodeAddress.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaMeshRouteKind {
    Unknown,
    Local,
    Remote,
    Relay,
    Multicast
};

struct MediaMeshRoute {
    std::string routeId;
    MediaMeshRouteKind kind = MediaMeshRouteKind::Unknown;
    MediaGraphNodeAddress source;
    std::vector<MediaGraphNodeAddress> targets;

    bool valid() const noexcept
    {
        return !routeId.empty() && source.valid() && !targets.empty();
    }
};

} // namespace media::ffmpeg::graph

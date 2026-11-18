#pragma once

#include "internal/graph/core/MediaGraph.h"
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionSourceTargets final {
    std::size_t sourceIndex;
    MediaNodeId primaryInput;
    std::optional<MediaNodeId> isolatedAudioInput;
    std::vector<MediaNodeId> sourceMembers;
};

} // namespace media::ffmpeg::graph

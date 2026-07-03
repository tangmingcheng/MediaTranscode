#pragma once

#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

namespace media::ffmpeg::graph {

class MediaRealtimeEdgePolicy final {
public:
    static MediaEdgePolicy make(const MediaRealtimeGraphBuilderOptions& options) noexcept;

private:
    MediaRealtimeEdgePolicy() = default;
};

} // namespace media::ffmpeg::graph

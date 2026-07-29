#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph {

class MediaRealtimeQueueCapacityPlanner final {
public:
    static MediaGraphQueueParameters plan(
        const MediaGraphQueueParameters& requested);

private:
    MediaRealtimeQueueCapacityPlanner() = delete;
};

} // namespace media::ffmpeg::graph

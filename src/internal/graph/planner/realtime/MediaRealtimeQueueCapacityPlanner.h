#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeQueueCapacityPlanner final {
public:
    static ::media::Result<MediaGraphQueueParameters> plan(
        const MediaGraphQueueParameters& requested);

private:
    MediaRealtimeQueueCapacityPlanner() = delete;
};

} // namespace media::ffmpeg::graph

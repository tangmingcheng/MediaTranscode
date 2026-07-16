#pragma once

#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph {

class MediaRealtimeEdgePolicyPlanner final {
public:
    static MediaRealtimeEdgePolicySet plan(
        const MediaGraphQueueParameters& queues);

private:
    MediaRealtimeEdgePolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph

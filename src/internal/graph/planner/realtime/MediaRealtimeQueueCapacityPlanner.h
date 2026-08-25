#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaRealtimeQueueCapacityPlanner final {
public:
    static ::media::Result<MediaGraphQueueParameters> plan(
        const MediaRealtimeDeploymentEnvelope& deployment,
        MediaRational outputFrameRate,
        std::optional<int> audioAccessUnitSamples,
        std::optional<int> audioSampleRate);

private:
    MediaRealtimeQueueCapacityPlanner() = delete;
};

} // namespace media::ffmpeg::graph

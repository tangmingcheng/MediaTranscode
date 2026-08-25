#pragma once

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaPreparedRealtimeEmissionSet final {
    MediaPreparedEncoderEmissionEnvelope video;
    std::optional<MediaPreparedAudioEncoderEmissionEnvelope> audio;
};

class MediaPreparedEmissionResolver final {
public:
    static ::media::Result<MediaPreparedRealtimeEmissionSet> resolve(
        const MediaPipelinePlan& videoPipeline,
        MediaRational outputFrameRate,
        const MediaAudioPipelinePlan* audioPipeline);

private:
    MediaPreparedEmissionResolver() = delete;
};

} // namespace media::ffmpeg::graph

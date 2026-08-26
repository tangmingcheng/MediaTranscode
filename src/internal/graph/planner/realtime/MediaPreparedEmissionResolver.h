#pragma once

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedHardwareMemoryEnvelope.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaPreparedRealtimeEmissionSet final {
    MediaPreparedEncoderEmissionEnvelope video;
    std::optional<MediaPreparedAudioEncoderEmissionEnvelope> audio;
    std::optional<MediaPreparedHardwareMemoryEnvelope> hardwareMemory;
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

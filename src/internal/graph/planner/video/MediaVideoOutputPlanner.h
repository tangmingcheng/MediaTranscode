#pragma once

#include "internal/graph/planner/video/MediaVideoOutputPlan.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaVideoOutputPlanner final {
public:
    static ::media::Result<MediaEncoderOpenContract> planOpenContract(
        const MediaPipelineStagePlan& encoder, MediaSize outputSize,
        MediaRational outputFrameRate, const MediaEncoderRateControlRequest& rateControl,
        const MediaVideoTranscodeParameters& request, bool lowLatency);

    static ::media::Status materializeExecutionContract(MediaVideoOutputPlan& output);
    static ::media::Status completePreparedRateControl(MediaVideoOutputPlan& output);

private:
    MediaVideoOutputPlanner() = delete;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/planner/video/MediaVideoOutputPlan.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaVideoOutputPlanner final {
public:
    static std::vector<MediaPipelineStagePlan> planCandidates(
        const std::string& codecName, MediaSize outputSize);

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

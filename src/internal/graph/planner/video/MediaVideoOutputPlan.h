#pragma once

#include "internal/graph/planner/video/MediaPipelineStagePlan.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"

namespace media::ffmpeg::graph {

// Encoder planning products for an already prepared raw-frame producer.
// No compressed source, decoder, transfer or source timing authority is implied.
struct MediaVideoOutputPlan {
    MediaPipelineStagePlan encoder;
    MediaVideoLineagePropagation encoderLineagePropagation = MediaVideoLineagePropagation::Unknown;
    MediaVideoEncoderAbortPolicy encoderAbortPolicy = MediaVideoEncoderAbortPolicy::Unknown;
};

} // namespace media::ffmpeg::graph

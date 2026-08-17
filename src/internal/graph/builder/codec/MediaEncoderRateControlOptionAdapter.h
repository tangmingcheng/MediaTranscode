#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "media_transcode/Result.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderRateControlOptionAdapter final {
public:
    static ::media::Result<MediaEncoderRateControlPlan> applyBeforeOpen(
        AVCodecContext& context,
        const MediaNodeOptions& options);
    static ::media::Status verifyAfterOpen(
        const AVCodecContext& context,
        const MediaEncoderRateControlPlan& plan);

private:
    MediaEncoderRateControlOptionAdapter() = delete;
};

} // namespace media::ffmpeg::graph

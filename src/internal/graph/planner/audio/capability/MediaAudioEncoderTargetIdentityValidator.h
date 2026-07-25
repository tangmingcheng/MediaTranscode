#pragma once

#include "internal/graph/planner/audio/MediaResolvedAudioTargetDecision.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

namespace media::ffmpeg::graph {

class MediaAudioEncoderTargetIdentityValidator final {
public:
    static ::media::Status validate(
        const MediaResolvedAudioTargetDecision& expected,
        AVSampleFormat expectedSampleFormat,
        const AVCodecContext& openedContext);

private:
    MediaAudioEncoderTargetIdentityValidator() = delete;
};

} // namespace media::ffmpeg::graph

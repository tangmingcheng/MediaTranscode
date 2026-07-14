#pragma once

#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/planner/audio/MediaResolvedAudioTargetDecision.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAudioEncoderCapabilityProvider final {
public:
    static ::media::Result<MediaSelectedAudioEncoder> verify(
        const MediaResolvedAudioTargetDecision& target);

private:
    MediaAudioEncoderCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph

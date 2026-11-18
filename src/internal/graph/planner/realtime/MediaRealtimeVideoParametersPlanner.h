#pragma once
#include "internal/graph/model/MediaTranscodeParameters.h"
#include <media_transcode/Result.h>
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
namespace media::ffmpeg::graph {
class MediaRealtimeVideoParametersPlanner final {
public:
    static ::media::Result<MediaVideoTranscodeParameters> plan(
        const MediaRealtimeVideoTranscodeParameters& requested,
        std::optional<RealtimeOutputStreamLayout> outputLayout);
};
} // namespace media::ffmpeg::graph

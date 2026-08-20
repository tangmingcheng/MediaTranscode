#pragma once

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpTranscodeRequest;

class MediaRealtimeAudioPlannerOptionsResolver final {
public:
    static ::media::Result<MediaAudioPipelinePlannerOptions> resolve(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeAudioPlannerOptionsResolver() = delete;
};

} // namespace media::ffmpeg::graph

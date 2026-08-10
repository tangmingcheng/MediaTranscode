#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <media_transcode/Result.h>

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeOutputUrls final {
    std::string video;
    std::string audio;
    std::string muxed;
};

class MediaRealtimeOutputPolicyPlanner final {
public:
    static ::media::Result<MediaRealtimeOutputUrls> planUrls(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Status apply(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeOutputUrls& urls,
        MediaRealtimeRtpTranscodePlanCore& plan,
        MediaRealtimeOutputPlanningDraft& output);

private:
    MediaRealtimeOutputPolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph

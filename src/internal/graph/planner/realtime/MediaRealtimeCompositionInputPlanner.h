#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionInputPlan final {
    MediaRealtimeInputConfig input;
    MediaTranscodeStreamSet members;
    // Two independent transport workers (RTP, RTCP) for each member stream.
    std::size_t creditWakeupOffset;
    std::size_t creditWakeupCount;
};

struct MediaRealtimeCompositionPreparationPlan final {
    std::vector<MediaRealtimeCompositionInputPlan> inputs;
    std::size_t storageLimitBytes;
    std::size_t creditWakeupCount;
};

class MediaRealtimeCompositionInputPlanner final {
public:
    static ::media::Result<MediaRealtimeCompositionPreparationPlan> plan(
        const MediaRealtimeCompositionConfig& composition);
};

} // namespace media::ffmpeg::graph

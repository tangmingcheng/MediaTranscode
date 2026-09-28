#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaAudioCorrectionReachabilityResult final {
    MediaAudioCorrectionReachabilityPlan correction;
    MediaRunningTime commandLead;
    MediaRunningTime compensationWindow;
    MediaRunningTime frequencyFilterTimeConstant;
    std::int64_t maximumOutputBlockSamples;
};

struct MediaAudioSourceCorrectionFacts final {
    int outputSampleRate;
    MediaSynchronizedAudioSourceBounds bounds;
};

using MediaAudioCorrectionPlanningFacts = std::variant<
    MediaRealtimeAvSyncPlanningFacts, MediaAudioSourceCorrectionFacts>;

class MediaAudioCorrectionReachabilityPlanner final {
public:
    static ::media::Result<MediaAudioCorrectionReachabilityResult> plan(
        const MediaAvSyncPlan& synchronization,
        const MediaAudioCorrectionPlanningFacts& facts);

private:
    MediaAudioCorrectionReachabilityPlanner() = delete;
};

} // namespace media::ffmpeg::graph

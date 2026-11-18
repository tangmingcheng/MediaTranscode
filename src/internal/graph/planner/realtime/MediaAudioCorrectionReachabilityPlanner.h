#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"

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

using MediaAudioCorrectionReachabilityAssessment = std::variant<
    MediaAudioCorrectionReachabilityResult, MediaAudioCorrectionCapacityRejection>;

class MediaAudioCorrectionReachabilityPlanner final {
public:
    // Exact membership assessment, not a continuous-k interval or byte budget.
    // Fixed facts must match across a search; arithmetic/fact failures propagate.
    static ::media::Result<MediaAudioCorrectionReachabilityAssessment> assessCapacity(
        const MediaAvSyncAudioServoPolicy& servo,
        const MediaAudioCorrectionPlanningFacts& facts);

    static ::media::Result<MediaAudioCorrectionReachabilityResult> plan(
        const MediaAvSyncAudioServoPolicy& servo,
        const MediaAudioCorrectionPlanningFacts& facts);

private:
    MediaAudioCorrectionReachabilityPlanner() = delete;
};

} // namespace media::ffmpeg::graph

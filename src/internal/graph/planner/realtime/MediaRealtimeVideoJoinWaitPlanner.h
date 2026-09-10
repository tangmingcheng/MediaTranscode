#pragma once
#include "internal/graph/model/MediaPreparedVideoRandomAccessEnvelope.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"

namespace media::ffmpeg::graph {
enum class MediaVideoJoinEncoderState { NewlyOpened, AlreadyRunning };
struct MediaRealtimeVideoJoinWaitPlan final {
    MediaRunningTime maximumIdrInterval;
    MediaRunningTime minimumFirstOutputBudget;
    MediaRunningTime maximumWait;
};
class MediaRealtimeVideoJoinWaitPlanner final {
public:
    static ::media::Result<MediaRealtimeVideoJoinWaitPlan> plan(
        const MediaPreparedVideoRandomAccessEnvelope& randomAccess,
        MediaVideoJoinEncoderState state, MediaRealtimeVideoRuntimePlan& runtime,
        MediaRunningTime remainingFirstOutputBudget);
    static ::media::Status validateAdmission(
        const MediaRealtimeVideoJoinWaitPlan& plan,
        MediaRunningTime remainingFirstOutputBudget);
};
} // namespace media::ffmpeg::graph

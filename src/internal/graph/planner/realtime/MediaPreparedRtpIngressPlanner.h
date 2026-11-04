#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
#include "internal/graph/planner/realtime/MediaRtpInputWaitPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressSnapshot.h"

namespace media::ffmpeg::graph {

class MediaPreparedRealtimeInput;
struct MediaRealtimeRtpTransportPlan;

struct MediaPreparedRtpIngressCandidate final {
    MediaPreparedRtpIngressSnapshot snapshot;
    MediaRtpIngressPlan plan;
};

enum class MediaPreparedRtpIngressPhase { SealedCapture, ContinuousSession };

class MediaPreparedRtpIngressPlanner final {
public:
    static ::media::Result<MediaPreparedRtpIngressCandidate> candidate(
        const MediaPreparedRealtimeInput& input, const MediaRtpInputWaitPlan& inputWait,
        MediaRtpIngressSnapshotWait wait);
    static ::media::Result<MediaRtpIngressPlan> plan(
        MediaPreparedRealtimeInput& input, const MediaRtpInputWaitPlan& inputWait,
        const MediaPreparedRtpIngressCandidate* candidate, MediaPreparedRtpIngressPhase phase);
    static ::media::Status bind(
        const MediaRtpIngressPlan& ingress,
        MediaRealtimeRtpTransportPlan& transport);

private:
    MediaPreparedRtpIngressPlanner() = delete;
};

} // namespace media::ffmpeg::graph

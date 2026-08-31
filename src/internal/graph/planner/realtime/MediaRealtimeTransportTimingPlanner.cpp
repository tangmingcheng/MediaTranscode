#include "internal/graph/planner/realtime/MediaRealtimeTransportTimingPlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeTransportTimingPlan>
MediaRealtimeTransportTimingPlanner::plan(
    const MediaRealtimeDeploymentLatencyBudget& latency)
{
    if (latency.maximumResidence <= MediaRunningTime::fromNanoseconds(0) ||
        latency.targetResidence <= MediaRunningTime::fromNanoseconds(0) ||
        latency.targetResidence > latency.maximumResidence ||
        latency.maximumReleaseJitter <= MediaRunningTime::fromNanoseconds(0) ||
        latency.maximumReleaseJitter >= latency.maximumResidence ||
        latency.authority.empty() || latency.releaseJitterAuthority.empty()) {
        return ::media::Result<MediaRealtimeTransportTimingPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime transport timing requires an authoritative wire latency budget"));
    }
    return ::media::Result<MediaRealtimeTransportTimingPlan>::success({
        latency.maximumResidence,
        latency.authority +
            "+sender-transport-lead-covers-immutable-wire-residence"});
}

} // namespace media::ffmpeg::graph

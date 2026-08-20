#include "internal/graph/planner/realtime/MediaScheduledDatagramPacingPlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaScheduledDatagramPacingPlan>
MediaScheduledDatagramPacingPlanner::plan(
    const MediaRtpUdpSenderConfig& transport)
{
    if (transport.ioBehavior() !=
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure) {
        return ::media::Result<MediaScheduledDatagramPacingPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled datagram pacing requires nonblocking userspace UDP"));
    }
    return ::media::Result<MediaScheduledDatagramPacingPlan>::success(
        MediaScheduledDatagramPacingPlan{
            MediaDatagramDispatchExecution::UserspaceWaitAndSend,
            MediaDatagramTimingEvidence::UserspaceSendReturn,
            MediaDatagramDeadlinePolicy::CanonicalOrdered});
}

} // namespace media::ffmpeg::graph

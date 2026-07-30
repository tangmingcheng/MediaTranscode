#include "internal/graph/sync/MediaAvSyncSharedNtpEpochRequirement.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"

namespace media::ffmpeg::graph {

::media::Result<bool>
MediaAvSyncSharedNtpEpochRequirement::resolve(
    const MediaAvSyncPlan& plan)
{
    if (plan.rtpOutput) {
        if (plan.projectMpegTsOutput ||
            !plan.rtpOutput->output.useSharedNtpEpoch) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Separate RTP output requires an explicit shared NTP epoch policy"));
        }
        return ::media::Result<bool>::success(
            *plan.rtpOutput->output.useSharedNtpEpoch);
    }
    if (plan.projectMpegTsOutput) {
        if (!plan.projectMpegTsOutput->useSharedNtpEpoch) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output requires an explicit shared NTP epoch policy"));
        }
        return ::media::Result<bool>::success(
            *plan.projectMpegTsOutput->useSharedNtpEpoch);
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::notInitialized(
            "A/V sync output authority is missing"));
}

} // namespace media::ffmpeg::graph

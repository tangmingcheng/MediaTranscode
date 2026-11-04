#include "internal/graph/protocol/rtp/MediaRtpSourceClockProjection.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRtpSourceClockProjection> projectMediaRtpSourceClockState(
    const MediaRtpClockGroupSnapshot& snapshot, MediaTranscodeStreamSet members)
{
    using Result = ::media::Result<MediaRtpSourceClockProjection>;
    if (auto status = snapshot.validateMembers(members); !status)
        return Result::failure(status.error());
    MediaSourceClockReadiness readiness;
    std::uint64_t projectedGeneration = snapshot.groupGeneration;
    bool discontinuity = false;
    switch (snapshot.state) {
    case MediaRtpClockGroupState::Acquiring:
        readiness = MediaSourceClockReadiness::Acquiring;
        break;
    case MediaRtpClockGroupState::Locked:
        if (snapshot.groupGeneration == 0 || !snapshot.locked) {
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Locked RTP clock snapshot is incomplete"));
        }
        readiness = MediaSourceClockReadiness::Locked;
        break;
    case MediaRtpClockGroupState::Degraded:
        readiness = MediaSourceClockReadiness::Degraded;
        break;
    case MediaRtpClockGroupState::ReacquireRequired:
        if (!snapshot.invalidatedGeneration ||
            *snapshot.invalidatedGeneration == 0 ||
            snapshot.groupGeneration <= *snapshot.invalidatedGeneration) {
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP clock invalidation requires its previous active generation"));
        }
        readiness = MediaSourceClockReadiness::ReacquireRequired;
        projectedGeneration = *snapshot.invalidatedGeneration;
        discontinuity = true;
        break;
    default:
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP source clock projection rejects unknown state"));
    }
    return Result::success({readiness, projectedGeneration, discontinuity, snapshot.evidenceRevision});
}

} // namespace media::ffmpeg::graph

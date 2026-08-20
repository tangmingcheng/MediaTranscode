#include "internal/graph/planner/MediaPipelineHardwareBackendConstraint.h"

namespace media::ffmpeg::graph {

::media::Status MediaPipelineHardwareBackendConstraint::validate(
    MediaHardwareBackendRequest request,
    bool disableHardware,
    const std::string& context)
{
    if (request == MediaHardwareBackendRequest::RKMPP && disableHardware) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            context + " rejects --hardware-backend rkmpp together with --disable-hw"));
    }
    return ::media::Status::success();
}

bool MediaPipelineHardwareBackendConstraint::accepts(
    const MediaPipelineChainPlan& candidate,
    bool filterRequired,
    MediaHardwareBackendRequest request) noexcept
{
    if (request == MediaHardwareBackendRequest::Auto) {
        return true;
    }

    return candidate.available && candidate.allHardware && candidate.sameHardwareDevice &&
        candidate.decoder.deviceKind() == MediaHardwareDeviceKind::RKMPP &&
        candidate.encoder.deviceKind() == MediaHardwareDeviceKind::RKMPP &&
           (!filterRequired ||
         candidate.filter.deviceKind() == MediaHardwareDeviceKind::RKMPP);
}

} // namespace media::ffmpeg::graph

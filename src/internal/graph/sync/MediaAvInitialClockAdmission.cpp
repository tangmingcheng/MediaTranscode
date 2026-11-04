#include "internal/graph/sync/MediaAvInitialClockAdmission.h"

namespace media::ffmpeg::graph {

::media::Status validateMediaInitialSourceClockAvailability(
    MediaAvSourceLifecycleMode mode, bool outputActivated,
    MediaSourceClockReadiness readiness, bool discontinuity)
{
    if (mode == MediaAvSourceLifecycleMode::PreserveActivatedOutput &&
        !outputActivated &&
        (discontinuity || readiness == MediaSourceClockReadiness::Degraded))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Initial composition source admission lost clock evidence"));
    return ::media::Status::success();
}

::media::Status validateMediaInitialClockGeneration(
    std::uint64_t generation, std::uint64_t plannedInitialGeneration)
{
    if (generation == 0 || plannedInitialGeneration == 0)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Locked packet gate requires exact nonzero generation authority"));
    if (generation != plannedInitialGeneration)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Locked packet gate rejects generation outside the planned initial acquisition"));
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

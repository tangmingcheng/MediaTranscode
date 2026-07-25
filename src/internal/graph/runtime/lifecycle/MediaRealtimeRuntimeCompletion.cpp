#include "internal/graph/runtime/lifecycle/MediaRealtimeRuntimeCompletion.h"

#include "internal/graph/runtime/MediaGraphRuntime.h"

namespace media::ffmpeg::graph {

MediaRealtimeRuntimeCompletionOutcome MediaRealtimeRuntimeCompletion::complete(
    MediaGraphRuntime& runtime,
    const ::media::Status& waitStatus)
{
    if (!runtime.threadedRunning()) {
        return {
            waitStatus
                ? ::media::Status::failure(::media::ErrorInfo::internalError(
                      "realtime runtime left the running state without a wait failure"))
                : ::media::Status::failure(waitStatus.error()),
            false
        };
    }

    const auto stopStatus = runtime.stop();
    if (const auto primaryFailure =
            runtime.threadedExecutor().primaryFailure()) {
        return {
            ::media::Status::failure(primaryFailure->error),
            true
        };
    }
    if (!waitStatus) {
        return { ::media::Status::failure(waitStatus.error()), true };
    }
    if (!stopStatus) {
        return { ::media::Status::failure(stopStatus.error()), true };
    }
    return { ::media::Status::success(), true };
}

} // namespace media::ffmpeg::graph

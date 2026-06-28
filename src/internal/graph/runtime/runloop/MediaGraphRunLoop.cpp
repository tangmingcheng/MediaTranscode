#include "internal/graph/runtime/runloop/MediaGraphRunLoop.h"

namespace media::ffmpeg::graph {

::media::Result<MediaGraphRunResult> MediaGraphRunLoop::run(MediaGraphRuntime& runtime,
                                                            bool startIfNeeded,
                                                            bool stopOnCompletion)
{
    if (!runtime.compiled()) {
        return ::media::Result<MediaGraphRunResult>::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRunLoop failed: runtime is not compiled"));
    }

    if (startIfNeeded && !runtime.running()) {
        auto status = runtime.start();
        if (!status) {
            return ::media::Result<MediaGraphRunResult>::failure(status.error());
        }
    }

    auto result = runtime.run();
    if (!result) {
        return result;
    }

    if (stopOnCompletion) {
        auto stopStatus = runtime.stop();
        if (!stopStatus) {
            return ::media::Result<MediaGraphRunResult>::failure(stopStatus.error());
        }
    }

    return result;
}

} // namespace media::ffmpeg::graph

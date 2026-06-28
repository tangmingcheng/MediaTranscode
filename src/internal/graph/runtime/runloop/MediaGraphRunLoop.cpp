#include "internal/graph/runtime/runloop/MediaGraphRunLoop.h"

namespace media::ffmpeg::graph {

::media::Result<MediaGraphRunLoopResult> MediaGraphRunLoop::runUntilIdle(
    MediaGraphRuntime& runtime,
    MediaGraphRunLoopConfig config)
{
    if (!runtime.compiled()) {
        return ::media::Result<MediaGraphRunLoopResult>::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRunLoop failed: runtime is not compiled"));
    }

    if (config.startIfNeeded && !runtime.running()) {
        auto status = runtime.start();
        if (!status) {
            return ::media::Result<MediaGraphRunLoopResult>::failure(status.error());
        }
    }

    MediaGraphRunLoopResult result;

    while (result.iterations < config.maxIterations) {
        const std::size_t before = queuedBufferCount(runtime);

        auto status = runtime.processOnce();
        if (!status) {
            return ::media::Result<MediaGraphRunLoopResult>::failure(status.error());
        }

        const std::size_t after = queuedBufferCount(runtime);
        ++result.iterations;

        if (before == 0 && after == 0) {
            ++result.idleIterations;
            if (result.idleIterations >= config.maxIdleIterations) {
                result.stoppedBecauseIdle = true;
                break;
            }
        } else {
            result.idleIterations = 0;
        }
    }

    if (config.stopOnCompletion) {
        auto stopStatus = runtime.stop();
        if (!stopStatus) {
            return ::media::Result<MediaGraphRunLoopResult>::failure(stopStatus.error());
        }
    }

    return ::media::Result<MediaGraphRunLoopResult>::success(result);
}

std::size_t MediaGraphRunLoop::queuedBufferCount(const MediaGraphRuntime& runtime)
{
    std::size_t count = 0;
    for (const MediaChannel* channel : runtime.context().channels().channels()) {
        if (channel) {
            count += channel->size();
        }
    }

    return count;
}

} // namespace media::ffmpeg::graph

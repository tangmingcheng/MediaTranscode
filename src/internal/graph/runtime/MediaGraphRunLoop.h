#pragma once

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "media_transcode/Result.h"

#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaGraphRunLoopConfig {
    std::size_t maxIterations = 10000;
    std::size_t maxIdleIterations = 4;
    bool startIfNeeded = true;
    bool stopOnCompletion = false;
};

struct MediaGraphRunLoopResult {
    std::size_t iterations = 0;
    std::size_t idleIterations = 0;
    bool stoppedBecauseIdle = false;
};

class MediaGraphRunLoop final {
public:
    static ::media::Result<MediaGraphRunLoopResult> runUntilIdle(
        MediaGraphRuntime& runtime,
        MediaGraphRunLoopConfig config = {});

private:
    static std::size_t queuedBufferCount(const MediaGraphRuntime& runtime);
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/runtime/MediaGraphRuntime.h"

namespace media::ffmpeg::graph {

class MediaGraphRuntimeLifecycleExecutor final {
public:
    static ::media::Result<MediaGraphRunResult> run(MediaGraphRuntime& runtime);
    static ::media::Status startThreaded(MediaGraphRuntime& runtime);
    static ::media::Status synchronizeThreadedState(MediaGraphRuntime& runtime);
    static ::media::Status flush(MediaGraphRuntime& runtime);
    static ::media::Status stop(MediaGraphRuntime& runtime);
    static void abort(MediaGraphRuntime& runtime) noexcept;
    static void reset(MediaGraphRuntime& runtime);

private:
    MediaGraphRuntimeLifecycleExecutor() = delete;
};

} // namespace media::ffmpeg::graph

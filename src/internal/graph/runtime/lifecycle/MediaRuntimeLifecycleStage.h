#pragma once

namespace media::ffmpeg::graph {

enum class MediaRuntimeLifecycleStage {
    Created,
    Compiled,
    Configured,
    Started,
    Processing,
    Flushing,
    Stopping,
    Stopped,
    Completed,
    Aborted,
    Failed
};

} // namespace media::ffmpeg::graph

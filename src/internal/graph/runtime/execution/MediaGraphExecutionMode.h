#pragma once

namespace media::ffmpeg::graph {

enum class MediaGraphExecutionMode {
    Manual,
    SingleThreadedRunLoop,
    ThreadedRuntime
};

} // namespace media::ffmpeg::graph

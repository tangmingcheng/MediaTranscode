#pragma once

namespace media::ffmpeg::graph {

enum class MediaGraphExecutionMode {
    Manual,
    SingleThreaded,
    ThreadedRuntime
};

} // namespace media::ffmpeg::graph

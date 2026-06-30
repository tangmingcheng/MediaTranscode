#pragma once

namespace media::ffmpeg::graph {

enum class MediaGraphExecutionMode {
    SingleThreaded,
    ThreadedRuntime
};

} // namespace media::ffmpeg::graph

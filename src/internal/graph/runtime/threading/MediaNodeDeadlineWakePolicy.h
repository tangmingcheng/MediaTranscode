#pragma once

namespace media::ffmpeg::graph {

enum class MediaNodeDeadlineWakePolicy {
    InputOrDeadline,
    DeadlineOrCancellation
};

} // namespace media::ffmpeg::graph

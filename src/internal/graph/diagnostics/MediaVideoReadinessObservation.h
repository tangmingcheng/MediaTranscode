#pragma once
#include <cstddef>
namespace media::ffmpeg::graph {
struct MediaVideoReadinessObservation final {
    enum class State { Completed, Ready, Pending, Unavailable, Error, Count };
    State state;
    const char* operation;
    int nativeCode = 0;
    int restoreCode = 0;
};
} // namespace media::ffmpeg::graph

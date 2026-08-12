#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

#include <atomic>
#include <chrono>

namespace media::ffmpeg::graph {

class MediaPreparedReadInterruptGuard final {
public:
    MediaPreparedReadInterruptGuard(
        AVFormatContext& context,
        std::chrono::steady_clock::time_point deadline) noexcept;
    MediaPreparedReadInterruptGuard(
        AVFormatContext& context,
        std::chrono::steady_clock::time_point deadline,
        const std::atomic_bool& stopRequested) noexcept;
    MediaPreparedReadInterruptGuard(
        const MediaPreparedReadInterruptGuard&) = delete;
    MediaPreparedReadInterruptGuard& operator=(
        const MediaPreparedReadInterruptGuard&) = delete;
    ~MediaPreparedReadInterruptGuard();

    void restore() noexcept;

private:
    struct State final {
        std::chrono::steady_clock::time_point deadline;
        const std::atomic_bool* stopRequested;
        AVIOInterruptCB previous;
    };

    static int interrupt(void* opaque) noexcept;
    MediaPreparedReadInterruptGuard(
        AVFormatContext& context,
        std::chrono::steady_clock::time_point deadline,
        const std::atomic_bool* stopRequested) noexcept;

    AVFormatContext* m_context;
    State m_state;
};

} // namespace media::ffmpeg::graph

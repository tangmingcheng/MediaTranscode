#pragma once

#include <atomic>
#include <cstdint>

namespace media::ffmpeg {

/**
 * @brief Interrupt controller used by realtime FFmpeg blocking operations.
 *
 * FFmpeg invokes interrupt_callback from avformat_open_input(),
 * avformat_find_stream_info() and av_read_frame() when those operations block.
 * The controller supports both explicit stop requests and per-operation
 * deadlines so the realtime engine can exit predictably.
 */
class FFmpegRealtimeInterruptController {
public:
    FFmpegRealtimeInterruptController() = default;

    FFmpegRealtimeInterruptController(const FFmpegRealtimeInterruptController&) = delete;
    FFmpegRealtimeInterruptController& operator=(const FFmpegRealtimeInterruptController&) = delete;

    void reset();
    void requestInterrupt();
    void clearInterruptRequest();

    void beginOperation(int timeoutMs);
    void endOperation();

    bool interrupted() const;
    bool interruptRequested() const;

    static int callback(void* opaque) noexcept;

private:
    static int64_t steadyNowMs();

private:
    std::atomic_bool m_interruptRequested{ false };
    std::atomic<int64_t> m_deadlineMs{ 0 };
};

} // namespace media::ffmpeg

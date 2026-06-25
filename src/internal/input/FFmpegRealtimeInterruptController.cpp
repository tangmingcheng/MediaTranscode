#include "internal/input/FFmpegRealtimeInterruptController.h"

#include <chrono>

namespace media::ffmpeg {

void FFmpegRealtimeInterruptController::reset()
{
    m_interruptRequested.store(false);
    m_deadlineMs.store(0);
}

void FFmpegRealtimeInterruptController::requestInterrupt()
{
    m_interruptRequested.store(true);
}

void FFmpegRealtimeInterruptController::clearInterruptRequest()
{
    m_interruptRequested.store(false);
}

void FFmpegRealtimeInterruptController::beginOperation(int timeoutMs)
{
    if (timeoutMs > 0) {
        m_deadlineMs.store(steadyNowMs() + timeoutMs);
    }
    else {
        m_deadlineMs.store(0);
    }
}

void FFmpegRealtimeInterruptController::endOperation()
{
    m_deadlineMs.store(0);
}

bool FFmpegRealtimeInterruptController::interrupted() const
{
    if (m_interruptRequested.load()) {
        return true;
    }

    const int64_t deadline = m_deadlineMs.load();
    return deadline > 0 && steadyNowMs() >= deadline;
}

bool FFmpegRealtimeInterruptController::interruptRequested() const
{
    return m_interruptRequested.load();
}

int FFmpegRealtimeInterruptController::callback(void* opaque) noexcept
{
    const auto* controller = static_cast<const FFmpegRealtimeInterruptController*>(opaque);
    return controller && controller->interrupted() ? 1 : 0;
}

int64_t FFmpegRealtimeInterruptController::steadyNowMs()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

} // namespace media::ffmpeg

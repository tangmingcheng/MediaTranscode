#include "internal/graph/planner/realtime/MediaPreparedReadInterruptGuard.h"

namespace media::ffmpeg::graph {

MediaPreparedReadInterruptGuard::MediaPreparedReadInterruptGuard(
    AVFormatContext& context,
    std::chrono::steady_clock::time_point deadline) noexcept
    : MediaPreparedReadInterruptGuard(context, deadline, nullptr)
{
}

MediaPreparedReadInterruptGuard::MediaPreparedReadInterruptGuard(
    AVFormatContext& context,
    std::chrono::steady_clock::time_point deadline,
    const std::atomic_bool& stopRequested) noexcept
    : MediaPreparedReadInterruptGuard(context, deadline, &stopRequested)
{
}

MediaPreparedReadInterruptGuard::MediaPreparedReadInterruptGuard(
    AVFormatContext& context,
    std::chrono::steady_clock::time_point deadline,
    const std::atomic_bool* stopRequested) noexcept
    : m_context(&context),
      m_state{deadline, stopRequested, context.interrupt_callback}
{
    context.interrupt_callback = AVIOInterruptCB{interrupt, &m_state};
}

MediaPreparedReadInterruptGuard::~MediaPreparedReadInterruptGuard()
{
    restore();
}

void MediaPreparedReadInterruptGuard::restore() noexcept
{
    if (!m_context) return;
    m_context->interrupt_callback = m_state.previous;
    m_context = nullptr;
}

int MediaPreparedReadInterruptGuard::interrupt(void* opaque) noexcept
{
    const auto* state = static_cast<const State*>(opaque);
    if (!state) return 1;
    if ((state->stopRequested &&
         state->stopRequested->load(std::memory_order_acquire)) ||
        std::chrono::steady_clock::now() >= state->deadline) {
        return 1;
    }
    return state->previous.callback
        ? state->previous.callback(state->previous.opaque)
        : 0;
}

} // namespace media::ffmpeg::graph

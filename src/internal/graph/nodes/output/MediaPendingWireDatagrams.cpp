#include "internal/graph/nodes/output/MediaPendingWireDatagrams.h"

namespace media::ffmpeg::graph {
::media::Status MediaPendingWireDatagrams::stage(MediaWireDatagramBatchCollection batches)
{
    if (!empty() || batches.empty()) return ::media::Status::failure(
        ::media::ErrorInfo::internalError("Wire publication requires an empty owner and nonempty collection"));
    for (const auto& batch : batches) {
        if (!batch) return ::media::Status::failure(
            ::media::ErrorInfo::internalError("Wire publication collection contains a null partition"));
    }
    m_batches = std::move(batches);
    return ::media::Status::success();
}

::media::Status MediaPendingWireDatagrams::stage(std::shared_ptr<MediaWireDatagramBatchBuffer> terminal)
{
    if (!empty() || !terminal) return ::media::Status::failure(
        ::media::ErrorInfo::internalError("Wire terminal publication requires an empty owner and nonnull report"));
    m_terminal = std::move(terminal);
    return ::media::Status::success();
}

void MediaPendingWireDatagrams::pop_front() noexcept
{
    if (!m_batches.empty()) {
        m_batches.pop_front();
        if (m_batches.empty()) m_batches = {};
    } else m_terminal.reset();
}

void MediaPendingWireDatagrams::clear() noexcept
{
    m_batches = {};
    m_terminal.reset();
}
} // namespace media::ffmpeg::graph

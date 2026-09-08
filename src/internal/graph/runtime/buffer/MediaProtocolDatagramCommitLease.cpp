#include "internal/graph/runtime/buffer/MediaProtocolDatagramCommitLease.h"

namespace media::ffmpeg::graph {

::media::Status MediaProtocolDatagramCommitTransaction::commitNextPrefix(
    std::size_t count) noexcept
{
    if (!m_reservation) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "protocol datagram commit transaction is inactive"));
    }
    if (count > m_size - m_committed) {
        m_reservation.reset();
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "protocol datagram commit prefix exceeds its reservation"));
    }
    if (count == 0) return ::media::Status::success();
    auto committed = m_reservation->commitNextPrefix(count);
    if (!committed) {
        m_reservation.reset();
        return committed;
    }
    m_committed += count;
    if (m_committed == m_size) m_reservation.reset();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

#include "internal/graph/runtime/buffer/MediaProtocolDatagramCommitLease.h"

namespace media::ffmpeg::graph {

::media::Status MediaProtocolDatagramCommitLease::commit() noexcept
{
    if (!m_reservation) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "protocol datagram commit lease is inactive"));
    }
    auto reservation = std::move(m_reservation);
    return reservation->commit();
}

} // namespace media::ffmpeg::graph

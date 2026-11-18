#pragma once
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/runtime/buffer/MediaProtocolDatagramCommitLease.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

namespace media::ffmpeg::graph {
class MediaMpegTsUdpWireCommitTransaction final {
public:
    static constexpr MediaDatagramCommitStorageKind storageKind() noexcept
    { return MediaDatagramCommitStorageKind::MpegTsUdp; }
    explicit MediaMpegTsUdpWireCommitTransaction(
        MediaWireGlobalSequenceReservation reservation,
        std::optional<MediaProtocolDatagramCommitTransaction>
            protocolCommit) noexcept
        : m_reservation(std::move(reservation)),
          m_protocolCommit(std::move(protocolCommit))
    {
    }

    std::size_t size() const noexcept { return m_reservation.size(); }

    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept
    {
        return m_reservation.sequence(index);
    }

    ::media::Status markScheduledPrefix(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept
    {
        if (begin > size() || count > size() - begin) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP schedule prefix is outside its transaction"));
        }
        return m_reservation.markScheduled(begin, count, now);
    }

    ::media::Status commitSubmittedPrefix(
        std::size_t begin,
        std::size_t count,
        const MediaDatagramSubmittedService& submitted) noexcept
    {
        if (begin > size() || count > size() - begin) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP submitted prefix is outside its transaction"));
        }
        if (m_protocolCommit &&
            (!m_protocolCommit->valid() ||
             count > m_protocolCommit->size() -
                         m_protocolCommit->committed())) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP submitted prefix exceeds its protocol transaction"));
        }
        auto committed = m_reservation.commit(begin, count, submitted);
        if (!committed) return committed;
        if (m_protocolCommit) {
            auto protocolCommitted = m_protocolCommit->commitNextPrefix(count);
            if (!protocolCommitted) return protocolCommitted;
        }
        return ::media::Status::success();
    }

private:
    MediaWireGlobalSequenceReservation m_reservation;
    std::optional<MediaProtocolDatagramCommitTransaction> m_protocolCommit;
};

} // namespace media::ffmpeg::graph

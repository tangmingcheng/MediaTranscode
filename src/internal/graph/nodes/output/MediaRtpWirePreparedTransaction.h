#pragma once
#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"
#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"

#include <array>
#include <type_traits>
#include <variant>

namespace media::ffmpeg::graph {
// Exclusive unpublished transaction. Output bytes are already serialized;
// no packetizer, input span, or stack serialization plan survives preparation.
class MediaRtpWirePreparedTransaction final {
public:
    MediaRtpWirePreparedTransaction(MediaRtpWirePreparedTransaction&&) noexcept = default;
    MediaRtpWirePreparedTransaction& operator=(MediaRtpWirePreparedTransaction&&) = delete;
    MediaRtpWirePreparedTransaction(const MediaRtpWirePreparedTransaction&) = delete;
    MediaRtpWirePreparedTransaction& operator=(const MediaRtpWirePreparedTransaction&) = delete;
    ~MediaRtpWirePreparedTransaction();
private:
    friend class MediaRtpWireDatagramMaterializer;
    using Entries = std::variant<std::array<MediaWireGlobalSequenceReservationEntry, 1>,
        MediaFixedCapacityStorage<MediaWireGlobalSequenceReservationEntry>>;
    static_assert(std::is_nothrow_constructible_v<Entries,
        MediaFixedCapacityStorage<MediaWireGlobalSequenceReservationEntry>&&>);
    std::span<const MediaWireGlobalSequenceReservationEntry> entries() const noexcept;
    MediaRtpWirePreparedTransaction(
        std::shared_ptr<MediaRtpWireProtocolState> state,
        MediaWireCommitPreparation storage,
        Entries entries,
        MediaFixedCapacityStorage<MediaRtpWireCommitAction> actions,
        MediaDatagramCommitTransaction::Prepared<MediaRtpWireCommitTransaction> control,
        std::optional<MediaRtcpSenderReportCommitToken> report,
        bool terminal, std::size_t mediaCount, std::uint64_t payloadOctets,
        std::optional<MediaRtpTimestamp> lastTimestamp, MediaRunningTime lastRelease,
        MediaRunningTime deadline,
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocol) noexcept;
    std::shared_ptr<MediaRtpWireProtocolState> m_state;
    MediaWireCommitPreparation m_storage;
    Entries m_entries;
    MediaFixedCapacityStorage<MediaRtpWireCommitAction> m_actions;
    MediaDatagramCommitTransaction::Prepared<MediaRtpWireCommitTransaction> m_control;
    std::optional<MediaRtcpSenderReportCommitToken> m_report;
    bool m_terminal;
    std::size_t m_mediaCount;
    std::uint64_t m_payloadOctets;
    std::optional<MediaRtpTimestamp> m_lastTimestamp;
    MediaRunningTime m_lastRelease;
    MediaRunningTime m_deadline;
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> m_protocol;
    std::size_t m_protocolOffset;
};
} // namespace media::ffmpeg::graph

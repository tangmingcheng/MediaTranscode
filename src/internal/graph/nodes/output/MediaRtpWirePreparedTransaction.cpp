#include "internal/graph/nodes/output/MediaRtpWirePreparedTransaction.h"

namespace media::ffmpeg::graph {
MediaRtpWirePreparedTransaction::MediaRtpWirePreparedTransaction(
    std::shared_ptr<MediaRtpWireProtocolState> state,
    MediaWireCommitPreparation storage,
    Entries entries,
    MediaFixedCapacityStorage<MediaRtpWireCommitAction> actions,
    MediaDatagramCommitTransaction::Prepared<MediaRtpWireCommitTransaction> control,
    std::optional<MediaRtcpSenderReportCommitToken> report,
    bool terminal, std::size_t mediaCount, std::uint64_t payloadOctets,
    std::optional<MediaRtpTimestamp> lastTimestamp, MediaRunningTime lastRelease,
    MediaRunningTime deadline,
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocol) noexcept
    : m_state(std::move(state)), m_storage(std::move(storage)),
      m_entries(std::move(entries)), m_actions(std::move(actions)),
      m_control(std::move(control)), m_report(std::move(report)),
      m_terminal(terminal), m_mediaCount(mediaCount), m_payloadOctets(payloadOctets),
      m_lastTimestamp(lastTimestamp), m_lastRelease(lastRelease),
      m_deadline(deadline), m_protocol(std::move(protocol)),
      m_protocolOffset(m_protocol ? m_protocol->materializedDatagrams() : 0) {}

std::span<const MediaWireGlobalSequenceReservationEntry>
MediaRtpWirePreparedTransaction::entries() const noexcept
{
    return std::visit([](const auto& values) -> std::span<const MediaWireGlobalSequenceReservationEntry> {
        if constexpr (std::is_same_v<std::decay_t<decltype(values)>,
            std::array<MediaWireGlobalSequenceReservationEntry, 1>>) return values;
        else return values.view();
    }, m_entries);
}

MediaRtpWirePreparedTransaction::~MediaRtpWirePreparedTransaction()
{
    if (m_state) {
        std::lock_guard lock(m_state->mutex);
        m_state->preparationActive = false;
    }
    if (m_protocol) m_protocol->abandonPreparedWire(m_protocolOffset);
}
} // namespace media::ffmpeg::graph

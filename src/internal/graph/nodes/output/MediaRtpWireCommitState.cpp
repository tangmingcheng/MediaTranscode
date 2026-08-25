#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t MaximumProtocolCounter =
    (std::numeric_limits<std::uint64_t>::max)();

class MediaRtpWireEntryReservation final {
public:
    MediaRtpWireEntryReservation(
        std::shared_ptr<MediaRtpWireCommitTransaction> transaction,
        std::size_t index) noexcept
        : m_transaction(std::move(transaction)), m_index(index)
    {
    }

    ::media::Status commit() noexcept
    {
        return m_transaction
            ? m_transaction->commit(m_index)
            : ::media::Status::failure(::media::ErrorInfo::internalError(
                  "RTP wire entry reservation has no transaction"));
    }

private:
    std::shared_ptr<MediaRtpWireCommitTransaction> m_transaction;
    std::size_t m_index;
};

} // namespace

MediaRtpWireProtocolState::MediaRtpWireProtocolState(
    MediaRtpWireDatagramMaterializerConfig config) noexcept
    : generation(config.generation),
      rtpEndpointId(config.rtpEndpointId),
      rtcpEndpointId(config.rtcpEndpointId),
      rtpDeadline(config.rtpDeadline),
      rtcpDeadline(config.rtcpDeadline),
      globalSequence(std::move(config.globalSequence)),
      identity(config.identity),
      clockMapper(config.clockMapper),
      ntpEpoch(config.ntpEpoch),
      senderReportSchedule(std::move(config.senderReportSchedule)),
      projectedSenderReportSchedule(senderReportSchedule),
      cname(std::move(config.cname)),
      nextRtpSequence(config.initialRtpSequence),
      packetCount(config.initialPacketCount),
      octetCount(config.initialOctetCount),
      projectedNextRtpSequence(config.initialRtpSequence),
      projectedPacketCount(config.initialPacketCount),
      projectedOctetCount(config.initialOctetCount),
      maximumDatagramBytes(config.maximumDatagramBytes),
      maximumOutstandingDatagrams(config.maximumOutstandingDatagrams)
{
}

MediaRtpWireCommitTransaction::MediaRtpWireCommitTransaction(
    std::shared_ptr<MediaRtpWireProtocolState> state,
    std::uint64_t reservationIdentity,
    MediaWireGlobalSequenceReservation globalReservation,
    std::vector<MediaRtpWireCommitAction> actions) noexcept
    : m_state(std::move(state)),
      m_reservationIdentity(reservationIdentity),
      m_globalReservation(std::move(globalReservation)),
      m_actions(std::move(actions))
{
}

MediaRtpWireCommitTransaction::~MediaRtpWireCommitTransaction() noexcept
{
    if (m_state && m_nextAction != m_actions.size()) {
        std::lock_guard lock(m_state->mutex);
        m_state->poisoned = true;
    }
}

::media::Result<std::uint64_t>
MediaRtpWireCommitTransaction::sequence(std::size_t index) const noexcept
{
    return m_globalReservation.sequence(index);
}

::media::Status MediaRtpWireCommitTransaction::commit(
    std::size_t index) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "RTP wire commit transaction is inactive"));
    }
    std::lock_guard lock(m_state->mutex);
    if (index != m_nextAction || index >= m_actions.size() ||
        m_state->poisoned || m_state->reservations.empty() ||
        m_state->reservations.front().identity != m_reservationIdentity ||
        m_state->reservations.front().actionCount != m_actions.size() ||
        m_state->reservations.front().committed != index) {
        return poison(::media::ErrorInfo::internalError(
            "RTP wire commit is stale, reordered, or inactive"));
    }
    auto globalReady = m_globalReservation.canCommit(index);
    if (!globalReady) return poison(globalReady.error());
    auto& action = m_actions[index];
    if (action.protocolCommit) {
        auto protocolCommitted = action.protocolCommit->commit();
        if (!protocolCommitted) return poison(protocolCommitted.error());
    }
    switch (action.kind) {
    case MediaRtpWireCommitActionKind::SenderReport:
        if (!action.reportToken) {
            return poison(::media::ErrorInfo::internalError(
                "RTP wire Sender Report commit lacks its schedule token"));
        }
        if (auto committed = m_state->senderReportSchedule.commit(
                *action.reportToken);
            !committed) {
            return poison(committed.error());
        }
        break;
    case MediaRtpWireCommitActionKind::Media:
        if (!action.timestamp || action.payloadOctets == 0 ||
            m_state->packetCount == MaximumProtocolCounter ||
            action.payloadOctets >
                MaximumProtocolCounter - m_state->octetCount) {
            return poison(::media::ErrorInfo::internalError(
                "RTP wire media commit differs from its prepared counters"));
        }
        ++m_state->nextRtpSequence;
        ++m_state->packetCount;
        m_state->octetCount += action.payloadOctets;
        m_state->lastCommittedTimestamp = *action.timestamp;
        break;
    case MediaRtpWireCommitActionKind::TerminalReport:
        if (m_state->terminalCommitted) {
            return poison(::media::ErrorInfo::internalError(
                "RTP terminal report was already committed"));
        }
        m_state->terminalCommitted = true;
        break;
    }
    auto globalCommitted = m_globalReservation.commit(index);
    if (!globalCommitted) return poison(globalCommitted.error());
    ++m_nextAction;
    ++m_state->reservations.front().committed;
    if (m_nextAction == m_actions.size()) {
        m_state->outstandingDatagrams -= m_actions.size();
        m_state->reservations.pop_front();
    }
    return ::media::Status::success();
}

::media::Status MediaRtpWireCommitTransaction::poison(
    ::media::ErrorInfo error) noexcept
{
    if (m_state) m_state->poisoned = true;
    return ::media::Status::failure(std::move(error));
}

::media::Result<MediaDatagramSubmitCommitLease> makeMediaRtpWireCommitLease(
    const std::shared_ptr<MediaRtpWireCommitTransaction>& transaction,
    std::size_t index,
    std::uint64_t generation,
    std::uint64_t globalSequence)
{
    return MediaDatagramSubmitCommitLease::create(
        generation,
        globalSequence,
        MediaRtpWireEntryReservation(transaction, index));
}

} // namespace media::ffmpeg::graph

#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"

#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
constexpr std::uint64_t MaximumProtocolCounter =
    (std::numeric_limits<std::uint64_t>::max)();

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
      maximumOutstandingDatagrams(config.maximumOutstandingDatagrams),
      batchPlan(config.batchPlan),
      reservationWakeup(std::move(config.reservationWakeup))
{
}

bool MediaRtpWireProtocolState::canAdmitReservation(
    std::size_t datagrams) const noexcept
{
    if (datagrams == 0) return false;
    return outstandingDatagrams <= maximumOutstandingDatagrams &&
           datagrams <= maximumOutstandingDatagrams - outstandingDatagrams;
}

MediaRtpWireCommitTransaction::MediaRtpWireCommitTransaction(
    std::shared_ptr<MediaRtpWireProtocolState> state,
    std::uint64_t reservationIdentity,
    MediaWireGlobalSequenceReservation globalReservation,
    std::vector<MediaRtpWireCommitAction> actions,
    std::optional<MediaProtocolDatagramCommitTransaction>
        protocolCommit) noexcept
    : m_state(std::move(state)),
      m_reservationIdentity(reservationIdentity),
      m_globalReservation(std::move(globalReservation)),
      m_actions(std::move(actions)),
      m_protocolCommit(std::move(protocolCommit))
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

::media::Status MediaRtpWireCommitTransaction::markScheduledPrefix(
    std::size_t begin,
    std::size_t count,
    MediaRunningTime now) noexcept
{
    if (begin > m_actions.size() || count > m_actions.size() - begin) {
        return poison(::media::ErrorInfo::internalError(
            "RTP wire schedule prefix is outside its transaction"));
    }
    auto marked = m_globalReservation.markScheduled(begin, count, now);
    return marked ? marked : poison(marked.error());
}

::media::Status MediaRtpWireCommitTransaction::commitSubmittedPrefix(
    std::size_t begin,
    std::size_t count,
    MediaRunningTime now) noexcept
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "RTP wire commit transaction is inactive"));
    }
    std::unique_lock lock(m_state->mutex);
    if (begin != m_nextAction || count == 0 ||
        begin > m_actions.size() || count > m_actions.size() - begin ||
        m_state->poisoned || m_state->reservations.empty() ||
        m_state->reservations.front().identity != m_reservationIdentity ||
        m_state->reservations.front().actionCount != m_actions.size() ||
        m_state->reservations.front().committed != begin) {
        return poisonLocked(::media::ErrorInfo::internalError(
            "RTP wire commit prefix is stale, reordered, or inactive"));
    }
    std::size_t protocolEntries = 0;
    auto packetCount = m_state->packetCount;
    auto octetCount = m_state->octetCount;
    auto nextRtpSequence = m_state->nextRtpSequence;
    auto lastTimestamp = m_state->lastCommittedTimestamp;
    auto terminalCommitted = m_state->terminalCommitted;
    for (std::size_t index = begin; index < begin + count; ++index) {
        const auto& action = m_actions[index];
        if (action.consumesProtocolEntry) {
            if (action.kind != MediaRtpWireCommitActionKind::Media ||
                protocolEntries == (std::numeric_limits<std::size_t>::max)()) {
                return poisonLocked(::media::ErrorInfo::internalError(
                    "RTP wire protocol entry mapping is invalid"));
            }
            ++protocolEntries;
        }
        switch (action.kind) {
        case MediaRtpWireCommitActionKind::SenderReport:
            if (!action.reportToken || action.timestamp ||
                action.payloadOctets != 0 || action.consumesProtocolEntry) {
                return poisonLocked(::media::ErrorInfo::internalError(
                    "RTP wire Sender Report action differs from its reservation"));
            }
            break;
        case MediaRtpWireCommitActionKind::Media:
            if (action.reportToken || !action.timestamp ||
                action.payloadOctets == 0 || packetCount == MaximumProtocolCounter ||
                action.payloadOctets > MaximumProtocolCounter - octetCount) {
                return poisonLocked(::media::ErrorInfo::internalError(
                    "RTP wire media prefix differs from its prepared counters"));
            }
            ++nextRtpSequence;
            ++packetCount;
            octetCount += action.payloadOctets;
            lastTimestamp = *action.timestamp;
            break;
        case MediaRtpWireCommitActionKind::TerminalReport:
            if (action.reportToken || action.timestamp ||
                action.payloadOctets != 0 || action.consumesProtocolEntry ||
                terminalCommitted) {
                return poisonLocked(::media::ErrorInfo::internalError(
                    "RTP terminal report action differs from its reservation"));
            }
            terminalCommitted = true;
            break;
        }
    }
    if (protocolEntries != 0) {
        if (!m_protocolCommit || !m_protocolCommit->valid() ||
            protocolEntries >
                m_protocolCommit->size() - m_protocolCommit->committed()) {
            return poisonLocked(::media::ErrorInfo::internalError(
                "RTP wire media prefix lacks its protocol transaction range"));
        }
    }
    auto globalCommitted = m_globalReservation.commit(begin, count, now);
    if (!globalCommitted) return poisonLocked(globalCommitted.error());
    if (protocolEntries != 0) {
        auto protocolCommitted =
            m_protocolCommit->commitNextPrefix(protocolEntries);
        if (!protocolCommitted) return poisonLocked(protocolCommitted.error());
    }
    for (std::size_t index = begin; index < begin + count; ++index) {
        const auto& action = m_actions[index];
        if (action.kind == MediaRtpWireCommitActionKind::SenderReport) {
            auto committed = m_state->senderReportSchedule.commit(
                *action.reportToken);
            if (!committed) return poisonLocked(committed.error());
        }
    }
    m_state->nextRtpSequence = nextRtpSequence;
    m_state->packetCount = packetCount;
    m_state->octetCount = octetCount;
    m_state->lastCommittedTimestamp = lastTimestamp;
    m_state->terminalCommitted = terminalCommitted;
    m_nextAction += count;
    m_state->reservations.front().committed += count;
    std::shared_ptr<MediaNodeWakeup> reservationWakeup;
    if (m_nextAction == m_actions.size()) {
        if (m_protocolCommit && m_protocolCommit->valid()) {
            return poisonLocked(::media::ErrorInfo::internalError(
                "RTP wire transaction completed before its nested protocol transaction"));
        }
        m_state->outstandingDatagrams -= m_actions.size();
        m_state->reservations.pop_front();
        if (m_state->blockedReservationDatagrams &&
            m_state->canAdmitReservation(
                *m_state->blockedReservationDatagrams)) {
            m_state->blockedReservationDatagrams.reset();
            reservationWakeup = m_state->reservationWakeup.lock();
        }
    }
    lock.unlock();
    if (reservationWakeup) reservationWakeup->notify();
    return ::media::Status::success();
}

::media::Status MediaRtpWireCommitTransaction::poison(
    ::media::ErrorInfo error) noexcept
{
    if (!m_state) return ::media::Status::failure(std::move(error));
    std::lock_guard lock(m_state->mutex);
    return poisonLocked(std::move(error));
}

::media::Status MediaRtpWireCommitTransaction::poisonLocked(
    ::media::ErrorInfo error) noexcept
{
    m_state->poisoned = true;
    return ::media::Status::failure(std::move(error));
}

} // namespace media::ffmpeg::graph

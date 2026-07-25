#include "internal/graph/nodes/mux/ScheduledRtpSenderSession.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

ScheduledRtcpDispatchDiagnostics::ScheduledRtcpDispatchDiagnostics(
    MediaRunningTime scheduledDeadline,
    MediaRunningTime reportInstant,
    MediaRunningTime nextDeadline,
    MediaRunningTime lateness,
    std::uint64_t skippedIntervals,
    MediaNtpWireTimestamp ntpTimestamp,
    MediaRtpTimestamp rtpTimestamp,
    ScheduledRtpSenderCounters senderCounters) noexcept
    : m_scheduledDeadline(scheduledDeadline),
      m_reportInstant(reportInstant),
      m_nextDeadline(nextDeadline),
      m_lateness(lateness),
      m_skippedIntervals(skippedIntervals),
      m_ntpTimestamp(ntpTimestamp),
      m_rtpTimestamp(rtpTimestamp),
      m_senderCounters(senderCounters)
{
}

ScheduledRtcpDispatchResult::ScheduledRtcpDispatchResult(
    MediaRunningTime nextDeadline) noexcept
    : m_kind(ScheduledRtcpDispatchKind::NotDue),
      m_nextDeadline(nextDeadline)
{
}

ScheduledRtcpDispatchResult::ScheduledRtcpDispatchResult(
    ScheduledRtcpDispatchDiagnostics diagnostics)
    : m_kind(ScheduledRtcpDispatchKind::Sent),
      m_nextDeadline(diagnostics.nextDeadline()),
      m_sent(std::move(diagnostics))
{
}

ScheduledRtpSenderSession::OperationGuard::OperationGuard(
    bool& operationActive) noexcept
    : m_operationActive(operationActive),
      m_entered(!operationActive)
{
    if (m_entered) m_operationActive = true;
}

ScheduledRtpSenderSession::OperationGuard::~OperationGuard()
{
    if (m_entered) m_operationActive = false;
}

ScheduledRtpSenderSession::ScheduledRtpSenderSession(
    ScheduledRtpSenderConfig config,
    ScheduledRtpDatagramSink rtpSink,
    ScheduledRtcpDatagramSink rtcpSink)
    : m_ntpEpoch(config.m_ntpEpoch),
      m_rtpMapper(config.m_rtpMapper),
      m_senderReportSchedule(std::move(config.m_senderReportSchedule)),
      m_cname(std::move(config.m_cname)),
      m_scheduleGeneration(config.m_scheduleGeneration),
      m_ssrc(config.m_streamConfig.identity().ssrc()),
      m_counters(config.m_initialCounters),
      m_rtpSink(std::move(rtpSink)),
      m_rtcpSink(std::move(rtcpSink)),
      m_pendingStreamConfig(std::move(config.m_streamConfig))
{
}

::media::Result<std::unique_ptr<ScheduledRtpSenderSession>>
ScheduledRtpSenderSession::create(
    ScheduledRtpSenderConfig config,
    ScheduledRtpDatagramSink rtpSink,
    ScheduledRtcpDatagramSink rtcpSink,
    ScheduledRtpPacketizerFactory& packetizerFactory)
{
    using SessionResult =
        ::media::Result<std::unique_ptr<ScheduledRtpSenderSession>>;
    if (!rtpSink || !rtcpSink) {
        return SessionResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender requires separate RTP and RTCP sinks"));
    }
    auto session = std::unique_ptr<ScheduledRtpSenderSession>(
        new (std::nothrow) ScheduledRtpSenderSession(
            std::move(config), std::move(rtpSink), std::move(rtcpSink)));
    if (!session) {
        return SessionResult::failure(
            ::media::ErrorInfo::allocationFailed(
                "ScheduledRtpSenderSession"));
    }
    auto packetizer = packetizerFactory.create(
        std::move(*session->m_pendingStreamConfig),
        [sender = session.get()](std::span<const std::uint8_t> datagram,
                                 std::size_t payloadOctets) {
            return sender->acceptRtpDatagram(datagram, payloadOctets);
        });
    session->m_pendingStreamConfig.reset();
    if (!packetizer) return SessionResult::failure(packetizer.error());
    if (!packetizer.value()) {
        return SessionResult::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP packetizer factory returned no session"));
    }
    session->m_packetizer = std::move(packetizer.value());
    return SessionResult::success(std::move(session));
}

::media::Status ScheduledRtpSenderSession::open()
{
    OperationGuard guard(m_operationActive);
    if (!guard.entered()) return operationRejected();
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_state != State::Created || !m_packetizer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender can be opened exactly once"));
    }
    auto opened = m_packetizer->open();
    if (!opened) {
        poison(opened.error());
        return ::media::Status::failure(*m_terminalFailure);
    }
    m_state = State::Open;
    return ::media::Status::success();
}

::media::Status ScheduledRtpSenderSession::sendAccessUnit(
    const AVPacket& packet,
    MediaRunningTime presentationOnMaster)
{
    OperationGuard guard(m_operationActive);
    if (!guard.entered()) return operationRejected();
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_state != State::Open || !m_packetizer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender must be open before access-unit delivery"));
    }
    auto timestamp = m_rtpMapper.map(presentationOnMaster);
    if (!timestamp) return ::media::Status::failure(timestamp.error());
    auto written = m_packetizer->writeAccessUnit(packet, timestamp.value());
    if (!written) {
        poison(written.error());
        return ::media::Status::failure(*m_terminalFailure);
    }
    return ::media::Status::success();
}

::media::Result<ScheduledRtcpDispatchResult>
ScheduledRtpSenderSession::dispatchSenderReport(MediaRunningTime now)
{
    using DispatchResult = ::media::Result<ScheduledRtcpDispatchResult>;
    OperationGuard guard(m_operationActive);
    if (!guard.entered()) {
        return DispatchResult::failure(operationRejected().error());
    }
    if (m_terminalFailure) {
        return DispatchResult::failure(*m_terminalFailure);
    }
    if (m_state != State::Open || !m_packetizer) {
        return DispatchResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender must be open before sender-report dispatch"));
    }
    auto prepared = m_senderReportSchedule.prepare(now, m_scheduleGeneration);
    if (!prepared) return DispatchResult::failure(prepared.error());
    if (!prepared.value()) {
        return DispatchResult::success(ScheduledRtcpDispatchResult(
            m_senderReportSchedule.nextDeadline()));
    }
    const auto& decision = *prepared.value();
    auto timestamp = MediaRtcpSenderReportGenerator::mapTimestamp(
        decision.reportInstant, m_ntpEpoch, m_rtpMapper);
    if (!timestamp) return DispatchResult::failure(timestamp.error());
    MediaRtcpSenderReportParameters parameters(
        m_ssrc,
        m_cname,
        timestamp.value(),
        m_counters.packetCount(),
        m_counters.octetCount());
    auto datagram = MediaRtcpSenderReportGenerator::serialize(parameters);
    if (!datagram) {
        poison(datagram.error());
        return DispatchResult::failure(*m_terminalFailure);
    }
    try {
        auto sent = m_rtcpSink(datagram.value());
        if (!sent) return DispatchResult::failure(sent.error());
    } catch (...) {
        poison(::media::ErrorInfo::internalError(
            "RTCP sink threw with ambiguous external side effect"));
        return DispatchResult::failure(*m_terminalFailure);
    }
    auto committed = m_senderReportSchedule.commit(decision.commitToken);
    if (!committed) {
        poison(::media::ErrorInfo::internalError(
            "RTCP sender report was accepted but schedule commit failed"));
        return DispatchResult::failure(*m_terminalFailure);
    }
    return DispatchResult::success(ScheduledRtcpDispatchResult(
        ScheduledRtcpDispatchDiagnostics(
            decision.scheduledDeadline,
            decision.reportInstant,
            decision.nextDeadline,
            decision.lateness,
            decision.skippedIntervals,
            timestamp.value().ntp().wire(),
            timestamp.value().rtp(),
            m_counters)));
}

::media::Status ScheduledRtpSenderSession::acceptRtpDatagram(
    std::span<const std::uint8_t> datagram,
    std::size_t payloadOctets)
{
    auto preflight = m_counters.preflight(payloadOctets);
    if (!preflight) return preflight;
    auto sent = m_rtpSink(datagram, payloadOctets);
    if (!sent) return sent;
    m_counters.commit(payloadOctets);
    return ::media::Status::success();
}

void ScheduledRtpSenderSession::poison(::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    m_state = State::Poisoned;
}

::media::Status ScheduledRtpSenderSession::operationRejected() const
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "scheduled RTP sender operations are non-reentrant"));
}

} // namespace media::ffmpeg::graph

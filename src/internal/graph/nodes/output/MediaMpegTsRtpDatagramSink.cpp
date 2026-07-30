#include "internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.h"

#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaMpegTsRtpDatagramSink::MediaMpegTsRtpDatagramSink(
    std::unique_ptr<MediaRtpUdpSenderTransport> transport,
    MediaMpegTsRtpPacketizer packetizer,
    MediaSharedNtpEpoch ntpEpoch,
    MediaRtcpSenderReportSchedule senderReportSchedule,
    ScheduledRtpSenderCounters counters,
    std::string cname,
    std::uint32_t ssrc,
    std::uint64_t generation) noexcept
    : m_transport(std::move(transport)),
      m_packetizer(std::move(packetizer)),
      m_ntpEpoch(ntpEpoch),
      m_senderReportSchedule(std::move(senderReportSchedule)),
      m_counters(counters),
      m_cname(std::move(cname)),
      m_ssrc(ssrc),
      m_generation(generation)
{
}

MediaMpegTsRtpDatagramSink::~MediaMpegTsRtpDatagramSink() noexcept
{
    m_transport.reset();
    m_closed = true;
}

::media::Result<std::unique_ptr<MediaMpegTsRtpDatagramSink>>
MediaMpegTsRtpDatagramSink::create(
    const MediaMpegTsRtpOutputPlan& plan,
    const MediaPlaybackEpoch& epoch,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    MediaUdpDatagramSenderPortFactory& portFactory)
{
    using SinkResult =
        ::media::Result<std::unique_ptr<MediaMpegTsRtpDatagramSink>>;
    if (epoch.generation == 0 || plan.cname().empty()) {
        return SinkResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T RTP sink requires a complete generation and CNAME"));
    }
    auto transportConfig = plan.transport().clone();
    if (!transportConfig) {
        return SinkResult::failure(transportConfig.error());
    }
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(transportConfig).value(), portFactory);
    if (!transport) return SinkResult::failure(transport.error());
    auto opened = transport.value()->open();
    if (!opened) return SinkResult::failure(opened.error());

    auto packetizer = MediaMpegTsRtpPacketizer::create(
        MediaMpegTsRtpPacketizerConfig{
            plan.payloadType(), plan.clockRate(), plan.ssrc(),
            plan.baseTimestamp(), plan.initialSequenceNumber(),
            plan.tsPacketsPerPayload(),
            plan.maximumDatagramBytes(), epoch.masterRelease});
    if (!packetizer) {
        (void)transport.value()->close();
        return SinkResult::failure(packetizer.error());
    }
    auto firstReport = epoch.masterRelease.checkedAdd(
        plan.senderReportInterval());
    if (!firstReport) {
        (void)transport.value()->close();
        return SinkResult::failure(firstReport.error());
    }
    auto reportSchedule = MediaRtcpSenderReportSchedule::create(
        firstReport.value(), plan.senderReportInterval(),
        plan.senderReportInterval(), epoch.generation);
    if (!reportSchedule) {
        (void)transport.value()->close();
        return SinkResult::failure(reportSchedule.error());
    }
    auto counters = ScheduledRtpSenderCounters::create(0, 0);
    if (!counters) {
        (void)transport.value()->close();
        return SinkResult::failure(counters.error());
    }
    auto sink = std::unique_ptr<MediaMpegTsRtpDatagramSink>(
        new (std::nothrow) MediaMpegTsRtpDatagramSink(
            std::move(transport).value(),
            std::move(packetizer).value(), sharedNtpEpoch,
            std::move(reportSchedule).value(), counters.value(),
            plan.cname(), plan.ssrc(), epoch.generation));
    if (!sink) {
        return SinkResult::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaMpegTsRtpDatagramSink"));
    }
    return SinkResult::success(std::move(sink));
}

::media::Status MediaMpegTsRtpDatagramSink::terminalStatus() const
{
    return ::media::Status::failure(*m_failure);
}

::media::Status MediaMpegTsRtpDatagramSink::fail(
    ::media::ErrorInfo error) noexcept
{
    if (!m_failure) m_failure = std::move(error);
    (void)closeTransport();
    return terminalStatus();
}

::media::Status MediaMpegTsRtpDatagramSink::dispatchSenderReport(
    MediaRunningTime now)
{
    auto prepared = m_senderReportSchedule.prepare(now, m_generation);
    if (!prepared) return ::media::Status::failure(prepared.error());
    if (!prepared.value()) return ::media::Status::success();
    const auto& decision = *prepared.value();
    auto timestamp = MediaRtcpSenderReportGenerator::mapTimestamp(
        decision.reportInstant, m_ntpEpoch,
        m_packetizer.clockMapper());
    if (!timestamp) {
        return ::media::Status::failure(timestamp.error());
    }
    auto datagram = MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            m_ssrc, m_cname, timestamp.value(),
            m_counters.packetCount(), m_counters.octetCount()));
    if (!datagram) {
        return ::media::Status::failure(datagram.error());
    }
    auto sent = sendRtcp(datagram.value());
    if (!sent) return sent;
    return m_senderReportSchedule.commit(decision.commitToken);
}

::media::Status MediaMpegTsRtpDatagramSink::sendRtcp(
    std::span<const std::uint8_t> datagram) noexcept
{
    try {
        return m_transport->sendRtcp(datagram);
    } catch (const MediaUdpAmbiguousDeliveryError& error) {
        return ::media::Status::failure(error.cause());
    } catch (...) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "MP2T RTCP transport threw during datagram delivery"));
    }
}

::media::Result<std::size_t> MediaMpegTsRtpDatagramSink::write(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime emitOnMaster)
{
    if (m_failure) {
        return ::media::Result<std::size_t>::failure(*m_failure);
    }
    if (m_closed || !m_transport) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "MP2T RTP sink is closed"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    if (m_lastEmitOnMaster &&
        emitOnMaster < *m_lastEmitOnMaster) {
        auto status = fail(::media::ErrorInfo::invalidArgument(
            "MP2T RTP sink emission time regressed"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    auto packet = m_packetizer.packetize(
        completeTsPackets, emitOnMaster);
    if (!packet) {
        auto status = fail(packet.error());
        return ::media::Result<std::size_t>::failure(status.error());
    }
    auto counterStatus = m_counters.preflight(
        packet.value().payloadOctets());
    if (!counterStatus) {
        auto status = fail(counterStatus.error());
        return ::media::Result<std::size_t>::failure(status.error());
    }
    auto report = dispatchSenderReport(emitOnMaster);
    if (!report) {
        auto status = fail(report.error());
        return ::media::Result<std::size_t>::failure(status.error());
    }
    try {
        auto sent = m_transport->sendRtp(packet.value().datagram());
        if (!sent) {
            auto status = fail(sent.error());
            return ::media::Result<std::size_t>::failure(status.error());
        }
    } catch (const MediaUdpAmbiguousDeliveryError& error) {
        auto status = fail(error.cause());
        return ::media::Result<std::size_t>::failure(status.error());
    } catch (...) {
        auto status = fail(::media::ErrorInfo::internalError(
            "MP2T RTP transport threw during datagram delivery"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    m_counters.commit(packet.value().payloadOctets());
    m_lastEmitOnMaster = emitOnMaster;
    return ::media::Result<std::size_t>::success(
        completeTsPackets.size());
}

::media::Status MediaMpegTsRtpDatagramSink::flush()
{
    if (m_failure) return terminalStatus();
    if (m_closed || !m_transport) {
        return fail(::media::ErrorInfo::notInitialized(
            "MP2T RTP sink cannot flush after close"));
    }
    return ::media::Status::success();
}

::media::Status MediaMpegTsRtpDatagramSink::sendTerminalReport()
{
    if (!m_lastEmitOnMaster) return ::media::Status::success();
    auto timestamp = MediaRtcpSenderReportGenerator::mapTimestamp(
        *m_lastEmitOnMaster, m_ntpEpoch,
        m_packetizer.clockMapper());
    if (!timestamp) {
        return ::media::Status::failure(timestamp.error());
    }
    auto datagram = MediaRtcpSenderReportGenerator::serializeWithBye(
        MediaRtcpSenderReportParameters(
            m_ssrc, m_cname, timestamp.value(),
            m_counters.packetCount(), m_counters.octetCount()));
    if (!datagram) {
        return ::media::Status::failure(datagram.error());
    }
    return sendRtcp(datagram.value());
}

::media::Status MediaMpegTsRtpDatagramSink::closeTransport() noexcept
{
    if (!m_transport) {
        m_closed = true;
        return ::media::Status::success();
    }
    auto transport = std::move(m_transport);
    m_closed = true;
    return transport->close();
}

::media::Status MediaMpegTsRtpDatagramSink::close()
{
    if (m_closed) {
        return m_failure ? terminalStatus() : ::media::Status::success();
    }
    if (!m_failure) {
        auto terminal = sendTerminalReport();
        if (!terminal && !m_failure) m_failure = terminal.error();
    }
    auto transportClosed = closeTransport();
    if (!transportClosed && !m_failure) {
        m_failure = transportClosed.error();
    }
    return m_failure ? terminalStatus() : ::media::Status::success();
}

} // namespace media::ffmpeg::graph

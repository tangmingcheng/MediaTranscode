#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"

#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t MaximumWireCounter =
    std::numeric_limits<std::uint32_t>::max();

int requiredClockRate(const ScheduledRtpMuxStreamConfig& stream) noexcept
{
    switch (stream.packetizationMode()) {
    case MediaScheduledRtpPacketizationMode::H264AnnexB:
        return 90'000;
    case MediaScheduledRtpPacketizationMode::AacLatm:
        return stream.codecParameters().sample_rate;
    }
    return 0;
}

} // namespace

ScheduledRtpSenderCounters::ScheduledRtpSenderCounters(
    std::uint64_t packetCount,
    std::uint64_t octetCount) noexcept
    : m_packetCount(packetCount),
      m_octetCount(octetCount)
{
}

::media::Result<ScheduledRtpSenderCounters>
ScheduledRtpSenderCounters::create(
    std::uint64_t packetCount,
    std::uint64_t octetCount) noexcept
{
    if (packetCount > MaximumWireCounter || octetCount > MaximumWireCounter) {
        return ::media::Result<ScheduledRtpSenderCounters>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender counters exceed RTCP wire width"));
    }
    return ::media::Result<ScheduledRtpSenderCounters>::success(
        ScheduledRtpSenderCounters(packetCount, octetCount));
}

::media::Status ScheduledRtpSenderCounters::preflight(
    std::size_t payloadOctets) const noexcept
{
    if (payloadOctets == 0 ||
        m_packetCount == MaximumWireCounter ||
        payloadOctets > MaximumWireCounter - m_octetCount) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender counter increment would overflow"));
    }
    return ::media::Status::success();
}

void ScheduledRtpSenderCounters::commit(std::size_t payloadOctets) noexcept
{
    ++m_packetCount;
    m_octetCount += payloadOctets;
}

ScheduledRtpSenderConfig::ScheduledRtpSenderConfig(
    ScheduledRtpMuxStreamConfig streamConfig,
    MediaSharedNtpEpoch ntpEpoch,
    MediaRtpOutputClockMapper rtpMapper,
    MediaRtcpSenderReportSchedule senderReportSchedule,
    std::string cname,
    std::uint64_t scheduleGeneration,
    ScheduledRtpSenderCounters initialCounters) noexcept
    : m_streamConfig(std::move(streamConfig)),
      m_ntpEpoch(ntpEpoch),
      m_rtpMapper(rtpMapper),
      m_senderReportSchedule(std::move(senderReportSchedule)),
      m_cname(std::move(cname)),
      m_scheduleGeneration(scheduleGeneration),
      m_initialCounters(initialCounters)
{
}

::media::Result<ScheduledRtpSenderConfig>
ScheduledRtpSenderConfig::create(
    ScheduledRtpMuxStreamConfig streamConfig,
    MediaSharedNtpEpoch ntpEpoch,
    MediaRtpOutputClockMapper rtpMapper,
    MediaRtcpSenderReportSchedule senderReportSchedule,
    std::string cname,
    std::uint64_t scheduleGeneration,
    ScheduledRtpSenderCounters initialCounters)
{
    if (scheduleGeneration == 0 ||
        scheduleGeneration != senderReportSchedule.generation()) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender generation must match its report schedule"));
    }
    auto cnameStatus = MediaRtcpSdesTextValidator::validateCname(cname);
    if (!cnameStatus) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            cnameStatus.error());
    }
    const int streamClockRate = requiredClockRate(streamConfig);
    if (streamClockRate <= 0 || rtpMapper.clockRate() != streamClockRate) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender mapper clock does not match packetization mode"));
    }
    auto baseAtOrigin = rtpMapper.map(rtpMapper.masterOrigin());
    if (!baseAtOrigin ||
        baseAtOrigin.value().wire() != rtpMapper.baseTimestamp() ||
        streamConfig.identity().ssrc() == 0) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP sender clock or stream identity is inconsistent"));
    }
    return ::media::Result<ScheduledRtpSenderConfig>::success(
        ScheduledRtpSenderConfig(
            std::move(streamConfig),
            ntpEpoch,
            rtpMapper,
            std::move(senderReportSchedule),
            std::move(cname),
            scheduleGeneration,
            initialCounters));
}

} // namespace media::ffmpeg::graph

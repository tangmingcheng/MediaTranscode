#include "internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.h"

#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int Mp2tPayloadType = 33;
constexpr int Mp2tClockRate = 90'000;

} // namespace

::media::Result<MediaMpegTsRtpOutputPlan>
MediaMpegTsRtpOutputPlan::create(
    MediaRtpUdpSenderConfig transport,
    std::string sdpPath,
    std::string sessionIdentity,
    MediaRunningTime senderReportInterval)
{
    const auto& rtp = transport.remoteRtpEndpoint();
    const auto& rtcp = transport.remoteRtcpEndpoint();
    const auto maximumDatagramBytes = transport.maximumDatagramBytes();
    auto packetCount =
        MediaTsMuxPlan::maximumPacketsPerRtpDatagram(maximumDatagramBytes);
    const auto addressFamily = rtp.addressFamily();
    const std::string numericAddress = rtp.numericAddress();
    const std::string outputIdentity =
        sessionIdentity + ".output.mp2t";
    const std::string cname =
        MediaRtpOutputIdentityPlanner::cname(sessionIdentity);
    auto sdpIdentity = MediaSdpSessionIdentity::create(
        sessionIdentity, 0, 0, sessionIdentity,
        rtp.addressFamily(), rtp.numericAddress(), cname);
    if (!packetCount || sdpPath.empty() || sessionIdentity.empty() ||
        senderReportInterval <= MediaRunningTime::fromNanoseconds(0) ||
        maximumDatagramBytes >
            static_cast<std::size_t>((std::numeric_limits<int>::max)() / 2) ||
        transport.sendBufferBytes() !=
            static_cast<int>(maximumDatagramBytes * 2) ||
        rtp.port() == 0 || (rtp.port() % 2) != 0 ||
        rtcp.port() != rtp.port() + 1 ||
        rtcp.addressFamily() != rtp.addressFamily() ||
        rtcp.numericAddress() != rtp.numericAddress() ||
        transport.localPortPolicy().kind() !=
            MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent ||
        transport.localPortPolicy().rtpPort() ||
        transport.localPortPolicy().rtcpPort() ||
        transport.localNumericAddress() !=
            (rtp.addressFamily() == MediaIpAddressFamily::Ipv4
                 ? "0.0.0.0"
                 : "::") ||
        transport.ioBehavior() !=
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure ||
        !sdpIdentity) {
        return ::media::Result<MediaMpegTsRtpOutputPlan>::failure(
            packetCount
                ? (sdpIdentity
                       ? ::media::ErrorInfo::invalidArgument(
                             "MPEG-TS RTP output plan is incomplete or inconsistent")
                       : sdpIdentity.error())
                : packetCount.error());
    }
    return ::media::Result<MediaMpegTsRtpOutputPlan>::success(
        MediaMpegTsRtpOutputPlan(
            std::move(transport), Mp2tPayloadType, Mp2tClockRate,
            MediaRtpOutputIdentityPlanner::stableNumeric(outputIdentity),
            MediaRtpOutputIdentityPlanner::stableNumeric(
                outputIdentity + ".timestamp"),
            MediaRtpOutputIdentityPlanner::stableSequenceNumber(
                outputIdentity + ".sequence"),
            cname, senderReportInterval, maximumDatagramBytes,
            packetCount.value(),
            MediaMpegTsRtpSdpPlan{
                std::move(sdpPath), sessionIdentity, sessionIdentity,
                addressFamily, numericAddress, cname}));
}

MediaMpegTsRtpOutputPlan::MediaMpegTsRtpOutputPlan(
    MediaRtpUdpSenderConfig transport,
    int payloadType,
    int clockRate,
    std::uint32_t ssrc,
    std::uint32_t baseTimestamp,
    std::uint16_t initialSequenceNumber,
    std::string cname,
    MediaRunningTime senderReportInterval,
    std::size_t maximumDatagramBytes,
    std::uint8_t tsPacketsPerPayload,
    MediaMpegTsRtpSdpPlan sdp) noexcept
    : m_transport(std::move(transport)),
      m_payloadType(payloadType),
      m_clockRate(clockRate),
      m_ssrc(ssrc),
      m_baseTimestamp(baseTimestamp),
      m_initialSequenceNumber(initialSequenceNumber),
      m_cname(std::move(cname)),
      m_senderReportInterval(senderReportInterval),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_tsPacketsPerPayload(tsPacketsPerPayload),
      m_sdp(std::move(sdp))
{
}

::media::Result<MediaMpegTsRtpOutputPlan>
MediaMpegTsRtpOutputPlan::clone() const
{
    auto transportClone = m_transport.clone();
    if (!transportClone) {
        return ::media::Result<MediaMpegTsRtpOutputPlan>::failure(
            transportClone.error());
    }
    return ::media::Result<MediaMpegTsRtpOutputPlan>::success(
        MediaMpegTsRtpOutputPlan(
            std::move(transportClone).value(),
            m_payloadType,
            m_clockRate,
            m_ssrc,
            m_baseTimestamp,
            m_initialSequenceNumber,
            m_cname,
            m_senderReportInterval,
            m_maximumDatagramBytes,
            m_tsPacketsPerPayload,
            m_sdp));
}

const MediaRtpUdpSenderConfig&
MediaMpegTsRtpOutputPlan::transport() const noexcept
{
    return m_transport;
}

int MediaMpegTsRtpOutputPlan::payloadType() const noexcept
{
    return m_payloadType;
}

int MediaMpegTsRtpOutputPlan::clockRate() const noexcept
{
    return m_clockRate;
}

std::uint32_t MediaMpegTsRtpOutputPlan::ssrc() const noexcept
{
    return m_ssrc;
}

std::uint32_t MediaMpegTsRtpOutputPlan::baseTimestamp() const noexcept
{
    return m_baseTimestamp;
}

std::uint16_t
MediaMpegTsRtpOutputPlan::initialSequenceNumber() const noexcept
{
    return m_initialSequenceNumber;
}

const std::string& MediaMpegTsRtpOutputPlan::cname() const noexcept
{
    return m_cname;
}

MediaRunningTime
MediaMpegTsRtpOutputPlan::senderReportInterval() const noexcept
{
    return m_senderReportInterval;
}

std::size_t
MediaMpegTsRtpOutputPlan::maximumDatagramBytes() const noexcept
{
    return m_maximumDatagramBytes;
}

std::uint8_t
MediaMpegTsRtpOutputPlan::tsPacketsPerPayload() const noexcept
{
    return m_tsPacketsPerPayload;
}

const MediaMpegTsRtpSdpPlan&
MediaMpegTsRtpOutputPlan::sdp() const noexcept
{
    return m_sdp;
}

} // namespace media::ffmpeg::graph

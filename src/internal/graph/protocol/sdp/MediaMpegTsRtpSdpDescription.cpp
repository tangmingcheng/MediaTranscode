#include "internal/graph/protocol/sdp/MediaMpegTsRtpSdpDescription.h"

#include "internal/graph/protocol/MediaUtf8TextValidator.h"
#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int Mp2tPayloadType = 33;
constexpr int Mp2tClockRate = 90'000;

const char* addressType(MediaIpAddressFamily family) noexcept
{
    switch (family) {
    case MediaIpAddressFamily::Ipv4:
        return "IP4";
    case MediaIpAddressFamily::Ipv6:
        return "IP6";
    }
    return nullptr;
}

void appendLine(std::string& output, std::string line)
{
    output += line;
    output += "\r\n";
}

} // namespace

MediaMpegTsRtpSdpDescription::MediaMpegTsRtpSdpDescription(
    std::string path,
    std::string originUsername,
    std::uint64_t sessionId,
    std::uint64_t sessionVersion,
    std::string sessionName,
    MediaIpAddressFamily addressFamily,
    std::string numericAddress,
    std::uint16_t rtpPort,
    std::uint16_t rtcpPort,
    int payloadType,
    int clockRate,
    std::uint32_t ssrc,
    std::string cname) noexcept
    : m_path(std::move(path)),
      m_originUsername(std::move(originUsername)),
      m_sessionId(sessionId),
      m_sessionVersion(sessionVersion),
      m_sessionName(std::move(sessionName)),
      m_addressFamily(addressFamily),
      m_numericAddress(std::move(numericAddress)),
      m_rtpPort(rtpPort),
      m_rtcpPort(rtcpPort),
      m_payloadType(payloadType),
      m_clockRate(clockRate),
      m_ssrc(ssrc),
      m_cname(std::move(cname))
{
}

::media::Result<MediaMpegTsRtpSdpDescription>
MediaMpegTsRtpSdpDescription::create(
    const MediaMpegTsRtpOutputPlan& plan,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    const MediaPlaybackEpoch& playbackEpoch)
{
    const auto& sdp = plan.sdp();
    const auto& rtp = plan.transport().remoteRtpEndpoint();
    const auto& rtcp = plan.transport().remoteRtcpEndpoint();
    auto capturedNtp = sharedNtpEpoch.map(
        sharedNtpEpoch.masterAtCapture());
    auto sessionIdentity = MediaSdpSessionIdentity::create(
        sdp.originUsername, 0, playbackEpoch.generation,
        sdp.sessionName, sdp.originAddressFamily,
        sdp.originNumericAddress, sdp.cname);
    if (!capturedNtp || !sessionIdentity ||
        playbackEpoch.generation == 0 || sdp.path.empty() ||
        plan.payloadType() != Mp2tPayloadType ||
        plan.clockRate() != Mp2tClockRate || plan.ssrc() == 0 ||
        plan.cname() != sdp.cname ||
        rtp.addressFamily() != sdp.originAddressFamily ||
        rtp.numericAddress() != sdp.originNumericAddress ||
        rtcp.addressFamily() != rtp.addressFamily() ||
        rtcp.numericAddress() != rtp.numericAddress() ||
        rtp.port() == 0 || (rtp.port() % 2) != 0 ||
        rtcp.port() != rtp.port() + 1 ||
        !MediaRtcpSdesTextValidator::validateCname(sdp.cname) ||
        !MediaUtf8TextValidator::validateNonControlText(
            sdp.sessionName, 255, "MP2T SDP session name")) {
        return ::media::Result<
            MediaMpegTsRtpSdpDescription>::failure(
            capturedNtp
                ? (sessionIdentity
                       ? ::media::ErrorInfo::invalidArgument(
                             "MP2T SDP plan is incomplete or inconsistent")
                       : sessionIdentity.error())
                : capturedNtp.error());
    }
    const auto wire = capturedNtp.value().wire();
    const std::uint64_t sessionId =
        (static_cast<std::uint64_t>(wire.seconds) << 32) |
        static_cast<std::uint64_t>(wire.fraction);
    return ::media::Result<MediaMpegTsRtpSdpDescription>::success(
        MediaMpegTsRtpSdpDescription(
            sdp.path, sdp.originUsername, sessionId,
            playbackEpoch.generation, sdp.sessionName,
            sdp.originAddressFamily, sdp.originNumericAddress,
            rtp.port(), rtcp.port(), plan.payloadType(),
            plan.clockRate(), plan.ssrc(), sdp.cname));
}

::media::Result<std::string>
MediaMpegTsRtpSdpDescription::serialize() const
{
    const char* type = addressType(m_addressFamily);
    if (!type) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::internalError(
                "MP2T SDP contains an unsupported address family"));
    }
    std::string output;
    output.reserve(512);
    appendLine(output, "v=0");
    appendLine(
        output,
        "o=" + m_originUsername + " " +
            std::to_string(m_sessionId) + " " +
            std::to_string(m_sessionVersion) + " IN " + type +
            std::string(" ") + m_numericAddress);
    appendLine(output, "s=" + m_sessionName);
    appendLine(output, "t=0 0");
    appendLine(
        output,
        "m=video " + std::to_string(m_rtpPort) +
            " RTP/AVP " + std::to_string(m_payloadType));
    appendLine(
        output,
        "c=IN " + std::string(type) + " " + m_numericAddress);
    appendLine(
        output,
        "a=rtcp:" + std::to_string(m_rtcpPort) +
            " IN " + type + std::string(" ") + m_numericAddress);
    appendLine(
        output,
        "a=rtpmap:" + std::to_string(m_payloadType) +
            " MP2T/" + std::to_string(m_clockRate));
    appendLine(
        output,
        "a=ssrc:" + std::to_string(m_ssrc) +
            " cname:" + m_cname);
    return ::media::Result<std::string>::success(std::move(output));
}

} // namespace media::ffmpeg::graph

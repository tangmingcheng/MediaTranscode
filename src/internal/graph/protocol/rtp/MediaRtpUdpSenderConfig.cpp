#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

bool isAdjacentPair(std::uint16_t rtpPort, std::uint16_t rtcpPort) noexcept
{
    return rtpPort != 0 && (rtpPort % 2) == 0 &&
           rtpPort < std::numeric_limits<std::uint16_t>::max() &&
           rtcpPort == static_cast<std::uint16_t>(rtpPort + 1);
}

} // namespace

MediaRtpUdpLocalPortPolicy::MediaRtpUdpLocalPortPolicy(
    MediaRtpUdpLocalPortPolicyKind kind,
    std::optional<std::uint16_t> rtpPort,
    std::optional<std::uint16_t> rtcpPort) noexcept
    : m_kind(kind), m_rtpPort(rtpPort), m_rtcpPort(rtcpPort)
{
}

::media::Result<MediaRtpUdpLocalPortPolicy>
MediaRtpUdpLocalPortPolicy::fixedAdjacent(
    std::uint16_t rtpPort, std::uint16_t rtcpPort)
{
    if (!isAdjacentPair(rtpPort, rtcpPort)) {
        return ::media::Result<MediaRtpUdpLocalPortPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "fixed local RTP port must be even and RTCP must be RTP + 1"));
    }
    return ::media::Result<MediaRtpUdpLocalPortPolicy>::success(
        MediaRtpUdpLocalPortPolicy(
            MediaRtpUdpLocalPortPolicyKind::FixedAdjacent, rtpPort, rtcpPort));
}

MediaRtpUdpLocalPortPolicy
MediaRtpUdpLocalPortPolicy::osAssignedIndependent() noexcept
{
    return MediaRtpUdpLocalPortPolicy(
        MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent,
        std::nullopt, std::nullopt);
}

MediaRtpUdpSenderConfig::MediaRtpUdpSenderConfig(
    MediaIpAddressFamily addressFamily,
    std::string localNumericAddress,
    MediaUdpDatagramEndpoint remoteRtpEndpoint,
    MediaUdpDatagramEndpoint remoteRtcpEndpoint,
    MediaRtpUdpLocalPortPolicy localPortPolicy,
    int sendBufferBytes,
    std::size_t maximumDatagramBytes,
    MediaUdpSenderIoBehavior ioBehavior) noexcept
    : m_addressFamily(addressFamily),
      m_localNumericAddress(std::move(localNumericAddress)),
      m_remoteRtpEndpoint(std::move(remoteRtpEndpoint)),
      m_remoteRtcpEndpoint(std::move(remoteRtcpEndpoint)),
      m_localPortPolicy(std::move(localPortPolicy)),
      m_sendBufferBytes(sendBufferBytes),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_ioBehavior(ioBehavior)
{
}

::media::Result<MediaRtpUdpSenderConfig> MediaRtpUdpSenderConfig::create(
    MediaIpAddressFamily addressFamily,
    std::string localNumericAddress,
    std::string remoteNumericAddress,
    std::uint16_t remoteRtpPort,
    std::uint16_t remoteRtcpPort,
    MediaRtpUdpLocalPortPolicy localPortPolicy,
    int sendBufferBytes,
    std::size_t maximumDatagramBytes,
    MediaUdpSenderIoBehavior ioBehavior)
{
    if (!isAdjacentPair(remoteRtpPort, remoteRtcpPort) ||
        sendBufferBytes <= 0 || maximumDatagramBytes == 0 ||
        maximumDatagramBytes > kMediaUdpMaximumPayloadBytes ||
        ioBehavior != MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "invalid explicit RTP UDP sender policy"));
    }
    auto local = MediaUdpDatagramEndpoint::create(
        addressFamily, localNumericAddress, 0);
    auto remoteRtp = MediaUdpDatagramEndpoint::create(
        addressFamily, remoteNumericAddress, remoteRtpPort);
    auto remoteRtcp = MediaUdpDatagramEndpoint::create(
        addressFamily, std::move(remoteNumericAddress), remoteRtcpPort);
    if (!local) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(local.error());
    }
    if (!remoteRtp) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(remoteRtp.error());
    }
    if (!remoteRtcp) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(remoteRtcp.error());
    }
    return ::media::Result<MediaRtpUdpSenderConfig>::success(
        MediaRtpUdpSenderConfig(
            addressFamily, std::move(localNumericAddress),
            std::move(remoteRtp.value()), std::move(remoteRtcp.value()),
            std::move(localPortPolicy), sendBufferBytes,
            maximumDatagramBytes, ioBehavior));
}

} // namespace media::ffmpeg::graph

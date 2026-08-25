#include "internal/graph/protocol/rtp/MediaRtpRemoteEndpointPair.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaRtpRemoteEndpointPair::MediaRtpRemoteEndpointPair(
    MediaUdpDatagramEndpoint remoteRtpEndpoint,
    MediaUdpDatagramEndpoint remoteRtcpEndpoint) noexcept
    : m_remoteRtpEndpoint(std::move(remoteRtpEndpoint)),
      m_remoteRtcpEndpoint(std::move(remoteRtcpEndpoint))
{
}

::media::Result<MediaRtpRemoteEndpointPair>
MediaRtpRemoteEndpointPair::create(
    MediaIpAddressFamily addressFamily,
    std::string remoteNumericAddress,
    std::uint16_t remoteRtpPort,
    std::uint16_t remoteRtcpPort)
{
    if (remoteRtpPort == 0 || (remoteRtpPort % 2) != 0 ||
        remoteRtpPort == std::numeric_limits<std::uint16_t>::max() ||
        remoteRtcpPort != static_cast<std::uint16_t>(remoteRtpPort + 1)) {
        return ::media::Result<MediaRtpRemoteEndpointPair>::failure(
            ::media::ErrorInfo::invalidArgument(
                "remote RTP port must be even and RTCP must be RTP + 1"));
    }
    auto rtp = MediaUdpDatagramEndpoint::create(
        addressFamily, remoteNumericAddress, remoteRtpPort);
    auto rtcp = MediaUdpDatagramEndpoint::create(
        addressFamily, std::move(remoteNumericAddress), remoteRtcpPort);
    if (!rtp) {
        return ::media::Result<MediaRtpRemoteEndpointPair>::failure(rtp.error());
    }
    if (!rtcp) {
        return ::media::Result<MediaRtpRemoteEndpointPair>::failure(rtcp.error());
    }
    return ::media::Result<MediaRtpRemoteEndpointPair>::success(
        MediaRtpRemoteEndpointPair(
            std::move(rtp).value(), std::move(rtcp).value()));
}

::media::Result<MediaRtpRemoteEndpointPair>
MediaRtpRemoteEndpointPair::clone() const
{
    return create(
        addressFamily(), m_remoteRtpEndpoint.numericAddress(),
        m_remoteRtpEndpoint.port(), m_remoteRtcpEndpoint.port());
}

bool operator==(const MediaRtpRemoteEndpointPair& left,
                const MediaRtpRemoteEndpointPair& right) noexcept
{
    return left.m_remoteRtpEndpoint == right.m_remoteRtpEndpoint &&
        left.m_remoteRtcpEndpoint == right.m_remoteRtcpEndpoint;
}

} // namespace media::ffmpeg::graph

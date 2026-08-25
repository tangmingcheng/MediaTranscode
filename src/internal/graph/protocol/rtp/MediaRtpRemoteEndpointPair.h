#pragma once

#include "internal/graph/runtime/network/MediaUdpDatagramEndpoint.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

class MediaRtpRemoteEndpointPair final {
public:
    static ::media::Result<MediaRtpRemoteEndpointPair> create(
        MediaIpAddressFamily addressFamily,
        std::string remoteNumericAddress,
        std::uint16_t remoteRtpPort,
        std::uint16_t remoteRtcpPort);
    ::media::Result<MediaRtpRemoteEndpointPair> clone() const;

    MediaRtpRemoteEndpointPair(MediaRtpRemoteEndpointPair&&) noexcept = default;
    MediaRtpRemoteEndpointPair& operator=(MediaRtpRemoteEndpointPair&&) noexcept = default;
    MediaRtpRemoteEndpointPair(const MediaRtpRemoteEndpointPair&) = delete;
    MediaRtpRemoteEndpointPair& operator=(const MediaRtpRemoteEndpointPair&) = delete;

    MediaIpAddressFamily addressFamily() const noexcept
    {
        return m_remoteRtpEndpoint.addressFamily();
    }
    const MediaUdpDatagramEndpoint& remoteRtpEndpoint() const noexcept
    {
        return m_remoteRtpEndpoint;
    }
    const MediaUdpDatagramEndpoint& remoteRtcpEndpoint() const noexcept
    {
        return m_remoteRtcpEndpoint;
    }

    friend bool operator==(const MediaRtpRemoteEndpointPair& left,
                           const MediaRtpRemoteEndpointPair& right) noexcept;

private:
    MediaRtpRemoteEndpointPair(
        MediaUdpDatagramEndpoint remoteRtpEndpoint,
        MediaUdpDatagramEndpoint remoteRtcpEndpoint) noexcept;

    MediaUdpDatagramEndpoint m_remoteRtpEndpoint;
    MediaUdpDatagramEndpoint m_remoteRtcpEndpoint;
};

} // namespace media::ffmpeg::graph

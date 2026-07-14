#pragma once

#include "internal/graph/runtime/network/MediaUdpDatagramSenderPort.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaRtpUdpLocalPortPolicyKind {
    FixedAdjacent,
    OsAssignedIndependent
};

class MediaRtpUdpLocalPortPolicy final {
public:
    static ::media::Result<MediaRtpUdpLocalPortPolicy> fixedAdjacent(
        std::uint16_t rtpPort, std::uint16_t rtcpPort);
    static MediaRtpUdpLocalPortPolicy osAssignedIndependent() noexcept;

    MediaRtpUdpLocalPortPolicyKind kind() const noexcept { return m_kind; }
    std::optional<std::uint16_t> rtpPort() const noexcept { return m_rtpPort; }
    std::optional<std::uint16_t> rtcpPort() const noexcept { return m_rtcpPort; }

private:
    MediaRtpUdpLocalPortPolicy(
        MediaRtpUdpLocalPortPolicyKind kind,
        std::optional<std::uint16_t> rtpPort,
        std::optional<std::uint16_t> rtcpPort) noexcept;

    MediaRtpUdpLocalPortPolicyKind m_kind;
    std::optional<std::uint16_t> m_rtpPort;
    std::optional<std::uint16_t> m_rtcpPort;
};

class MediaRtpUdpSenderConfig final {
public:
    static ::media::Result<MediaRtpUdpSenderConfig> create(
        MediaIpAddressFamily addressFamily,
        std::string localNumericAddress,
        std::string remoteNumericAddress,
        std::uint16_t remoteRtpPort,
        std::uint16_t remoteRtcpPort,
        MediaRtpUdpLocalPortPolicy localPortPolicy,
        int sendBufferBytes,
        std::size_t maximumDatagramBytes,
        MediaUdpSenderIoBehavior ioBehavior);

    MediaRtpUdpSenderConfig(MediaRtpUdpSenderConfig&&) noexcept = default;
    MediaRtpUdpSenderConfig& operator=(MediaRtpUdpSenderConfig&&) noexcept = default;
    MediaRtpUdpSenderConfig(const MediaRtpUdpSenderConfig&) = delete;
    MediaRtpUdpSenderConfig& operator=(const MediaRtpUdpSenderConfig&) = delete;

    MediaIpAddressFamily addressFamily() const noexcept { return m_addressFamily; }
    const MediaUdpDatagramEndpoint& remoteRtpEndpoint() const noexcept
    {
        return m_remoteRtpEndpoint;
    }
    const MediaUdpDatagramEndpoint& remoteRtcpEndpoint() const noexcept
    {
        return m_remoteRtcpEndpoint;
    }
    std::size_t maximumDatagramBytes() const noexcept
    {
        return m_maximumDatagramBytes;
    }

private:
    friend class MediaRtpUdpSenderTransport;

    MediaRtpUdpSenderConfig(
        MediaIpAddressFamily addressFamily,
        std::string localNumericAddress,
        MediaUdpDatagramEndpoint remoteRtpEndpoint,
        MediaUdpDatagramEndpoint remoteRtcpEndpoint,
        MediaRtpUdpLocalPortPolicy localPortPolicy,
        int sendBufferBytes,
        std::size_t maximumDatagramBytes,
        MediaUdpSenderIoBehavior ioBehavior) noexcept;

    MediaIpAddressFamily m_addressFamily;
    std::string m_localNumericAddress;
    MediaUdpDatagramEndpoint m_remoteRtpEndpoint;
    MediaUdpDatagramEndpoint m_remoteRtcpEndpoint;
    MediaRtpUdpLocalPortPolicy m_localPortPolicy;
    int m_sendBufferBytes;
    std::size_t m_maximumDatagramBytes;
    MediaUdpSenderIoBehavior m_ioBehavior;
};

} // namespace media::ffmpeg::graph

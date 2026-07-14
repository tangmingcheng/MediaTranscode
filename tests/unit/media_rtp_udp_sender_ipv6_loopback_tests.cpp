#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace media::ffmpeg::graph;

#ifdef _WIN32
namespace {

SOCKET bindIpv6Receiver(std::uint16_t port)
{
    SOCKET socketHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) return INVALID_SOCKET;
    const DWORD timeoutMilliseconds = 2'000;
    const DWORD ipv6Only = 1;
    if (setsockopt(socketHandle, IPPROTO_IPV6, IPV6_V6ONLY,
                   reinterpret_cast<const char*>(&ipv6Only), sizeof(ipv6Only)) ==
            SOCKET_ERROR ||
        setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeoutMilliseconds),
                   sizeof(timeoutMilliseconds)) == SOCKET_ERROR) {
        closesocket(socketHandle);
        return INVALID_SOCKET;
    }
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    address.sin6_port = htons(port);
    InetPtonA(AF_INET6, "::1", &address.sin6_addr);
    if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        closesocket(socketHandle);
        return INVALID_SOCKET;
    }
    return socketHandle;
}

bool receiveDatagram(SOCKET socketHandle, std::span<const std::uint8_t> expected)
{
    std::array<std::uint8_t, 256> buffer{};
    const int received = recvfrom(
        socketHandle, reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(buffer.size()), 0, nullptr, nullptr);
    return received == static_cast<int>(expected.size()) &&
           std::equal(expected.begin(), expected.end(), buffer.begin());
}

} // namespace
#endif

int main()
{
    auto runtime = MediaSocketRuntime::create();
    if (!runtime) {
        std::cerr << "UNSUPPORTED: " << runtime.error().describe() << '\n';
        return 77;
    }
#ifndef _WIN32
    std::cerr << "UNSUPPORTED: Windows Winsock IPv6 loopback test only\n";
    return 77;
#else
    SOCKET rtpReceiver = INVALID_SOCKET;
    SOCKET rtcpReceiver = INVALID_SOCKET;
    std::uint16_t rtpPort = 0;
    for (std::uint32_t port = 20'000; port <= 65'000; port += 2) {
        rtpReceiver = bindIpv6Receiver(static_cast<std::uint16_t>(port));
        if (rtpReceiver == INVALID_SOCKET) continue;
        rtcpReceiver = bindIpv6Receiver(static_cast<std::uint16_t>(port + 1));
        if (rtcpReceiver != INVALID_SOCKET) {
            rtpPort = static_cast<std::uint16_t>(port);
            break;
        }
        closesocket(rtpReceiver);
        rtpReceiver = INVALID_SOCKET;
    }
    if (rtpReceiver == INVALID_SOCKET || rtcpReceiver == INVALID_SOCKET) {
        std::cerr << "UNSUPPORTED: IPv6 loopback or adjacent ports unavailable\n";
        return 77;
    }
    const auto closeReceivers = [&] {
        closesocket(rtpReceiver);
        closesocket(rtcpReceiver);
    };
    auto config = MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv6, "::1", "::1", rtpPort,
        static_cast<std::uint16_t>(rtpPort + 1),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        1 << 20, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
    if (!config) {
        closeReceivers();
        return 1;
    }
    MediaUdpDatagramSenderSocketFactory factory(runtime.value());
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    if (!transport || !transport.value()->open()) {
        std::cerr << "IPv6 sender transport create/open failed after receiver bind\n";
        closeReceivers();
        return 1;
    }
    const std::vector<std::uint8_t> rtp{0x80, 0x60, 1, 2};
    const std::vector<std::uint8_t> rtcp{0x80, 0xC8, 3, 4};
    const bool passed = transport.value()->sendRtp(rtp) &&
        transport.value()->sendRtcp(rtcp) &&
        receiveDatagram(rtpReceiver, rtp) &&
        receiveDatagram(rtcpReceiver, rtcp);
    closeReceivers();
    return passed ? 0 : 1;
#endif
}

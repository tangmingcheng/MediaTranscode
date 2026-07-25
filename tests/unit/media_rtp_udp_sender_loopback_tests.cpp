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

class ReceiverPair final {
public:
    ~ReceiverPair()
    {
        if (m_rtp != INVALID_SOCKET) closesocket(m_rtp);
        if (m_rtcp != INVALID_SOCKET) closesocket(m_rtcp);
    }

    bool open()
    {
        for (std::uint32_t port = 20'000; port <= 65'000; port += 2) {
            SOCKET rtp = bindReceiver(static_cast<std::uint16_t>(port));
            if (rtp == INVALID_SOCKET) continue;
            SOCKET rtcp = bindReceiver(static_cast<std::uint16_t>(port + 1));
            if (rtcp == INVALID_SOCKET) {
                closesocket(rtp);
                continue;
            }
            m_rtp = rtp;
            m_rtcp = rtcp;
            m_rtpPort = static_cast<std::uint16_t>(port);
            return true;
        }
        return false;
    }

    std::uint16_t rtpPort() const noexcept { return m_rtpPort; }

    bool receiveRtp(std::span<const std::uint8_t> expected)
    {
        return receive(m_rtp, expected);
    }

    bool receiveRtcp(std::span<const std::uint8_t> expected)
    {
        return receive(m_rtcp, expected);
    }

private:
    static SOCKET bindReceiver(std::uint16_t port)
    {
        SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketHandle == INVALID_SOCKET) return INVALID_SOCKET;
        const DWORD timeoutMilliseconds = 2'000;
        if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeoutMilliseconds),
                       sizeof(timeoutMilliseconds)) == SOCKET_ERROR) {
            closesocket(socketHandle);
            return INVALID_SOCKET;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        InetPtonA(AF_INET, "127.0.0.1", &address.sin_addr);
        if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&address),
                 sizeof(address)) == SOCKET_ERROR) {
            closesocket(socketHandle);
            return INVALID_SOCKET;
        }
        return socketHandle;
    }

    static bool receive(SOCKET socketHandle,
                        std::span<const std::uint8_t> expected)
    {
        std::array<std::uint8_t, 256> buffer{};
        const int received = recvfrom(
            socketHandle, reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()), 0, nullptr, nullptr);
        return received == static_cast<int>(expected.size()) &&
               std::equal(expected.begin(), expected.end(), buffer.begin());
    }

    SOCKET m_rtp = INVALID_SOCKET;
    SOCKET m_rtcp = INVALID_SOCKET;
    std::uint16_t m_rtpPort = 0;
};

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
    std::cerr << "UNSUPPORTED: Windows Winsock loopback test only\n";
    return 77;
#else
    ReceiverPair receivers;
    if (!receivers.open()) {
        std::cerr << "UNSUPPORTED: no adjacent IPv4 loopback receiver pair available\n";
        return 77;
    }
    auto config = MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv4,
        "127.0.0.1",
        "127.0.0.1",
        receivers.rtpPort(),
        static_cast<std::uint16_t>(receivers.rtpPort() + 1),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        1 << 20,
        1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
    if (!config) {
        std::cerr << config.error().describe() << '\n';
        return 1;
    }
    MediaUdpDatagramSenderSocketFactory factory(runtime.value());
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    if (!transport || !transport.value()->open()) {
        std::cerr << "loopback sender transport open failed\n";
        return 1;
    }
    const auto bound = transport.value()->boundLocalEndpoints();
    if (!bound || bound->rtp().port() == 0 || bound->rtcp().port() == 0 ||
        bound->rtp().port() == bound->rtcp().port()) {
        std::cerr << "OS-assigned sender endpoints are not distinct\n";
        return 1;
    }
    const std::vector<std::uint8_t> rtp{0x80, 0x60, 1, 2, 3, 4};
    const std::vector<std::uint8_t> rtcp{0x80, 0xC8, 5, 6, 7, 8};
    if (!transport.value()->sendRtp(rtp) ||
        !transport.value()->sendRtcp(rtcp) ||
        !receivers.receiveRtp(rtp) ||
        !receivers.receiveRtcp(rtcp)) {
        std::cerr << "RTP/RTCP loopback routing failed\n";
        return 1;
    }
    if (!transport.value()->close()) return 1;

    auto local = MediaUdpDatagramEndpoint::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 0);
    auto remote = MediaUdpDatagramEndpoint::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", receivers.rtpPort());
    if (!local || !remote) return 1;
    auto request = MediaUdpDatagramSenderPortOpenRequest::create(
        std::move(local.value()), std::move(remote.value()),
        1 << 20, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
    if (!request) return 1;
    MediaUdpDatagramSenderSocket oneShot(runtime.value());
    if (!oneShot.open(request.value())) return 1;
    oneShot.close();
    const auto reopened = oneShot.open(request.value());
    if (reopened || reopened.error().code != ::media::ErrorCode::InvalidArgument) {
        std::cerr << "sender socket reopened after close\n";
        return 1;
    }

    auto conflictingLocal = MediaUdpDatagramEndpoint::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", receivers.rtpPort());
    auto conflictingRemote = MediaUdpDatagramEndpoint::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", receivers.rtpPort());
    if (!conflictingLocal || !conflictingRemote) return 1;
    auto conflictingRequest = MediaUdpDatagramSenderPortOpenRequest::create(
        std::move(conflictingLocal.value()), std::move(conflictingRemote.value()),
        1 << 20, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
    if (!conflictingRequest) return 1;
    MediaUdpDatagramSenderSocket failedOneShot(runtime.value());
    const auto failedOpen = failedOneShot.open(conflictingRequest.value());
    if (failedOpen || failedOpen.error().code != ::media::ErrorCode::IoFailure) {
        std::cerr << "exclusive fixed-port conflict was not rejected\n";
        return 1;
    }
    const auto retriedFailedOpen = failedOneShot.open(conflictingRequest.value());
    if (retriedFailedOpen ||
        retriedFailedOpen.error().code != ::media::ErrorCode::InvalidArgument) {
        std::cerr << "sender socket retried after failed open\n";
        return 1;
    }
    return 0;
#endif
}

#include "internal/graph/planner/realtime/MediaRtpIngressPlatformCapabilityProbe.h"

#ifndef _WIN32

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class SocketHandle final {
public:
    explicit SocketHandle(int value) noexcept : m_value(value) {}
    ~SocketHandle()
    {
        if (m_value >= 0) ::close(m_value);
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    int get() const noexcept { return m_value; }

private:
    int m_value;
};

MediaRtpIngressAdapterAvailability probeIoUringZeroCopy()
{
    constexpr auto kind = MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy;
    return {kind, false,
            "this build has no authoritative kernel, device, and registered-memory io_uring zero-copy RX adapter"};
}

MediaRtpIngressAdapterAvailability probeReceiveMultipleMessages()
{
    constexpr auto kind =
        MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages;
    SocketHandle socketHandle(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0));
    if (socketHandle.get() < 0) {
        return {kind, false, "nonblocking UDP socket creation failed with errno " +
            std::to_string(errno)};
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socketHandle.get(), reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0) {
        return {kind, false, "UDP loopback bind failed with errno " +
            std::to_string(errno)};
    }
    std::uint8_t byte = 0;
    iovec vector{&byte, sizeof(byte)};
    mmsghdr message{};
    message.msg_hdr.msg_iov = &vector;
    message.msg_hdr.msg_iovlen = 1;
    const int received = recvmmsg(
        socketHandle.get(), &message, 1, MSG_DONTWAIT, nullptr);
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return {kind, true, {}};
    }
    if (received < 0) {
        return {kind, false, "recvmmsg capability probe failed with errno " +
            std::to_string(errno)};
    }
    return {kind, false,
            "recvmmsg capability probe returned data on an isolated socket"};
}

} // namespace

::media::Result<std::vector<MediaRtpIngressAdapterAvailability>>
MediaRtpIngressPlatformCapabilityProbe::scan()
{
    std::vector<MediaRtpIngressAdapterAvailability> candidates;
    candidates.reserve(2);
    candidates.push_back(probeIoUringZeroCopy());
    candidates.push_back(probeReceiveMultipleMessages());
    return ::media::Result<
        std::vector<MediaRtpIngressAdapterAvailability>>::success(
            std::move(candidates));
}

} // namespace media::ffmpeg::graph

#endif

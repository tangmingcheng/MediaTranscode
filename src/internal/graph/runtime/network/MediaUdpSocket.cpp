#include "internal/graph/runtime/network/MediaUdpSocket.h"

#include <limits>
#include <cstring>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace media::ffmpeg::graph {

struct MediaUdpSocket::Impl final {
    std::shared_ptr<MediaSocketRuntime> runtime;
#ifdef _WIN32
    SOCKET socket;
    WSAEVENT event;
#endif
    MediaIpAddressFamily family;
    uint16_t port;
};

namespace {

#ifdef _WIN32
int nativeFamily(MediaIpAddressFamily family)
{
    return family == MediaIpAddressFamily::Ipv4 ? AF_INET : AF_INET6;
}

std::string normalizedAddress(const std::string& address)
{
    if (address.size() >= 2 && address.front() == '[' && address.back() == ']') {
        return address.substr(1, address.size() - 2);
    }
    return address;
}

::media::Status makeAddress(MediaIpAddressFamily family, const std::string& address, uint16_t port,
                            sockaddr_storage& storage, int& size)
{
    const std::string text = normalizedAddress(address);
    if (family == MediaIpAddressFamily::Ipv4) {
        sockaddr_in value{};
        value.sin_family = AF_INET;
        value.sin_port = htons(port);
        if (InetPtonA(AF_INET, text.c_str(), &value.sin_addr) != 1) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Invalid IPv4 bind address"));
        }
        std::memcpy(&storage, &value, sizeof(value));
        size = sizeof(value);
        return ::media::Status::success();
    }
    sockaddr_in6 value{};
    value.sin6_family = AF_INET6;
    value.sin6_port = htons(port);
    if (InetPtonA(AF_INET6, text.c_str(), &value.sin6_addr) != 1) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Invalid IPv6 bind address"));
    }
    std::memcpy(&storage, &value, sizeof(value));
    size = sizeof(value);
    return ::media::Status::success();
}
#endif

} // namespace

MediaUdpSocket::MediaUdpSocket() noexcept = default;
MediaUdpSocket::MediaUdpSocket(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
MediaUdpSocket::~MediaUdpSocket() { close(); }
MediaUdpSocket::MediaUdpSocket(MediaUdpSocket&&) noexcept = default;
MediaUdpSocket& MediaUdpSocket::operator=(MediaUdpSocket&& other) noexcept
{
    if (this != &other) {
        close();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

::media::Result<MediaUdpSocket> MediaUdpSocket::bind(
    std::shared_ptr<MediaSocketRuntime> runtime, const MediaUdpSocketConfig& config)
{
    if (!runtime) {
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::invalidArgument("UDP socket requires initialized socket runtime"));
    }
    if (config.bindAddress.empty() || config.receiveBufferBytes <= 0) {
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::invalidArgument("UDP bind address and receive buffer must be explicit"));
    }
#ifdef _WIN32
    const SOCKET socketHandle = ::socket(nativeFamily(config.addressFamily), SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::ioFailure("UDP socket creation failed", WSAGetLastError()));
    }
    auto closeOnFailure = [&socketHandle] { closesocket(socketHandle); };
    const BOOL exclusive = TRUE;
    if (setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR ||
        setsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&config.receiveBufferBytes),
                   sizeof(config.receiveBufferBytes)) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        closeOnFailure();
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::ioFailure("UDP socket option configuration failed", error));
    }
    sockaddr_storage address{};
    int addressSize = 0;
    if (auto status = makeAddress(config.addressFamily, config.bindAddress, config.port,
                                  address, addressSize); !status) {
        closeOnFailure();
        return ::media::Result<MediaUdpSocket>::failure(status.error());
    }
    if (::bind(socketHandle, reinterpret_cast<const sockaddr*>(&address), addressSize) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        closeOnFailure();
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::ioFailure("UDP socket bind failed", error));
    }
    sockaddr_storage local{};
    int localSize = sizeof(local);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&local), &localSize) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        closeOnFailure();
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::ioFailure("UDP socket local endpoint query failed", error));
    }
    const uint16_t localPort = local.ss_family == AF_INET
        ? ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port)
        : ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port);
    const WSAEVENT event = WSACreateEvent();
    if (event == WSA_INVALID_EVENT || WSAEventSelect(socketHandle, event, FD_READ | FD_CLOSE) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        if (event != WSA_INVALID_EVENT) WSACloseEvent(event);
        closeOnFailure();
        return ::media::Result<MediaUdpSocket>::failure(
            ::media::ErrorInfo::ioFailure("UDP socket event registration failed", error));
    }
    return ::media::Result<MediaUdpSocket>::success(MediaUdpSocket(std::make_unique<Impl>(
        Impl{std::move(runtime), socketHandle, event, config.addressFamily, localPort})));
#else
    (void)config;
    return ::media::Result<MediaUdpSocket>::failure(
        ::media::ErrorInfo::unsupported("UDP socket runtime is currently implemented for Windows"));
#endif
}

bool MediaUdpSocket::isOpen() const noexcept
{
#ifdef _WIN32
    return m_impl && m_impl->socket != INVALID_SOCKET;
#else
    return false;
#endif
}

uint16_t MediaUdpSocket::localPort() const noexcept
{
    return m_impl ? m_impl->port : 0;
}

::media::Status MediaUdpSocket::sendTo(const std::string& address, uint16_t port,
                                       std::span<const uint8_t> datagram) const
{
    if (!isOpen() || datagram.empty() ||
        datagram.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("UDP send requires open socket and non-empty bounded datagram"));
    }
#ifdef _WIN32
    sockaddr_storage destination{};
    int destinationSize = 0;
    if (auto status = makeAddress(m_impl->family, address, port, destination, destinationSize); !status) return status;
    const int sent = sendto(m_impl->socket, reinterpret_cast<const char*>(datagram.data()),
                            static_cast<int>(datagram.size()), 0,
                            reinterpret_cast<const sockaddr*>(&destination), destinationSize);
    if (sent != static_cast<int>(datagram.size())) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure("UDP datagram send failed", WSAGetLastError()));
    }
    return ::media::Status::success();
#else
    return ::media::Status::failure(::media::ErrorInfo::unsupported("UDP send is unavailable"));
#endif
}

::media::Result<std::vector<uint8_t>> MediaUdpSocket::receive(std::size_t maximumDatagramBytes)
{
    if (!isOpen() || maximumDatagramBytes == 0 ||
        maximumDatagramBytes > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return ::media::Result<std::vector<uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument("UDP receive requires open socket and bounded datagram size"));
    }
#ifdef _WIN32
    std::vector<uint8_t> bytes(maximumDatagramBytes);
    const int received = recvfrom(m_impl->socket, reinterpret_cast<char*>(bytes.data()),
                                  static_cast<int>(bytes.size()), 0, nullptr, nullptr);
    if (received == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        return ::media::Result<std::vector<uint8_t>>::failure(
            error == WSAEWOULDBLOCK
                ? ::media::ErrorInfo::wouldBlock("UDP socket has no datagram after event")
                : ::media::ErrorInfo::ioFailure("UDP receive failed", error));
    }
    bytes.resize(static_cast<std::size_t>(received));
    return ::media::Result<std::vector<uint8_t>>::success(std::move(bytes));
#else
    return ::media::Result<std::vector<uint8_t>>::failure(::media::ErrorInfo::unsupported("UDP receive is unavailable"));
#endif
}

void* MediaUdpSocket::waitHandle() const noexcept
{
#ifdef _WIN32
    return isOpen() ? m_impl->event : nullptr;
#else
    return nullptr;
#endif
}

void MediaUdpSocket::consumeNetworkEvent() noexcept
{
#ifdef _WIN32
    if (isOpen()) {
        WSANETWORKEVENTS events{};
        WSAEnumNetworkEvents(m_impl->socket, m_impl->event, &events);
    }
#endif
}

void MediaUdpSocket::close() noexcept
{
#ifdef _WIN32
    if (m_impl) {
        if (m_impl->socket != INVALID_SOCKET) {
            WSAEventSelect(m_impl->socket, nullptr, 0);
            closesocket(m_impl->socket);
            m_impl->socket = INVALID_SOCKET;
        }
        if (m_impl->event != WSA_INVALID_EVENT) {
            WSACloseEvent(m_impl->event);
            m_impl->event = WSA_INVALID_EVENT;
        }
    }
#endif
    m_impl.reset();
}

} // namespace media::ffmpeg::graph

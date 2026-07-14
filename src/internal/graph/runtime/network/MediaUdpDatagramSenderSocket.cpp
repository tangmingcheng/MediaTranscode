#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

struct MediaUdpDatagramSenderSocket::Impl final {
    explicit Impl(std::shared_ptr<MediaSocketRuntime> socketRuntime)
        : runtime(std::move(socketRuntime))
    {
    }

    std::shared_ptr<MediaSocketRuntime> runtime;
#ifdef _WIN32
    SOCKET socket = INVALID_SOCKET;
    sockaddr_storage remoteAddress{};
    int remoteAddressLength = 0;
#endif
    std::optional<MediaUdpDatagramEndpoint> localEndpoint;
    std::optional<MediaUdpDatagramEndpoint> remoteEndpoint;
    std::size_t maximumDatagramBytes = 0;
    bool openAttempted = false;
};

#ifdef _WIN32
namespace {

::media::Status fillSockaddr(const MediaUdpDatagramEndpoint& endpoint,
                             sockaddr_storage& storage,
                             int& length)
{
    std::memset(&storage, 0, sizeof(storage));
    if (endpoint.addressFamily() == MediaIpAddressFamily::Ipv4) {
        auto* address = reinterpret_cast<sockaddr_in*>(&storage);
        address->sin_family = AF_INET;
        address->sin_port = htons(endpoint.port());
        if (InetPtonA(AF_INET, endpoint.numericAddress().c_str(),
                      &address->sin_addr) != 1) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "invalid numeric IPv4 UDP sender address"));
        }
        length = sizeof(sockaddr_in);
        return ::media::Status::success();
    }
    auto* address = reinterpret_cast<sockaddr_in6*>(&storage);
    address->sin6_family = AF_INET6;
    address->sin6_port = htons(endpoint.port());
    if (InetPtonA(AF_INET6, endpoint.numericAddress().c_str(),
                  &address->sin6_addr) != 1) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "invalid numeric IPv6 UDP sender address"));
    }
    length = sizeof(sockaddr_in6);
    return ::media::Status::success();
}

::media::Result<MediaUdpDatagramEndpoint> endpointFromSockaddr(
    const sockaddr_storage& storage)
{
    std::array<char, INET6_ADDRSTRLEN> addressText{};
    if (storage.ss_family == AF_INET) {
        const auto* address = reinterpret_cast<const sockaddr_in*>(&storage);
        if (!InetNtopA(AF_INET, const_cast<IN_ADDR*>(&address->sin_addr),
                       addressText.data(), static_cast<DWORD>(addressText.size()))) {
            return ::media::Result<MediaUdpDatagramEndpoint>::failure(
                ::media::ErrorInfo::ioFailure(
                    "InetNtop failed for bound IPv4 sender endpoint",
                    WSAGetLastError()));
        }
        return MediaUdpDatagramEndpoint::create(
            MediaIpAddressFamily::Ipv4, addressText.data(), ntohs(address->sin_port));
    }
    if (storage.ss_family == AF_INET6) {
        const auto* address = reinterpret_cast<const sockaddr_in6*>(&storage);
        if (!InetNtopA(AF_INET6, const_cast<IN6_ADDR*>(&address->sin6_addr),
                       addressText.data(), static_cast<DWORD>(addressText.size()))) {
            return ::media::Result<MediaUdpDatagramEndpoint>::failure(
                ::media::ErrorInfo::ioFailure(
                    "InetNtop failed for bound IPv6 sender endpoint",
                    WSAGetLastError()));
        }
        return MediaUdpDatagramEndpoint::create(
            MediaIpAddressFamily::Ipv6, addressText.data(), ntohs(address->sin6_port));
    }
    return ::media::Result<MediaUdpDatagramEndpoint>::failure(
        ::media::ErrorInfo::internalError(
            "bound UDP sender endpoint has an unknown address family"));
}

} // namespace
#endif

MediaUdpDatagramSenderSocket::MediaUdpDatagramSenderSocket(
    std::shared_ptr<MediaSocketRuntime> runtime)
    : m_impl(std::make_unique<Impl>(std::move(runtime)))
{
}

MediaUdpDatagramSenderSocket::~MediaUdpDatagramSenderSocket()
{
    close();
}

::media::Status MediaUdpDatagramSenderSocket::open(
    const MediaUdpDatagramSenderPortOpenRequest& request)
{
#ifdef _WIN32
    if (!m_impl->runtime) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "UDP sender socket requires an explicit socket runtime"));
    }
    if (m_impl->openAttempted) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "UDP sender socket can be opened exactly once"));
    }
    m_impl->openAttempted = true;
    const int family = request.localEndpoint().addressFamily() ==
            MediaIpAddressFamily::Ipv4
        ? AF_INET
        : AF_INET6;
    SOCKET socketHandle = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "UDP sender socket creation failed", WSAGetLastError()));
    }
    const auto fail = [&socketHandle](::media::ErrorInfo error) {
        closesocket(socketHandle);
        socketHandle = INVALID_SOCKET;
        return ::media::Status::failure(std::move(error));
    };
    const BOOL exclusiveAddressUse = TRUE;
    if (setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusiveAddressUse),
                   sizeof(exclusiveAddressUse)) == SOCKET_ERROR) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender SO_EXCLUSIVEADDRUSE configuration failed",
            WSAGetLastError()));
    }
    const int sendBufferBytes = request.sendBufferBytes();
    if (setsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<const char*>(&sendBufferBytes),
                   sizeof(sendBufferBytes)) == SOCKET_ERROR) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender SO_SNDBUF configuration failed", WSAGetLastError()));
    }
    u_long nonBlocking = 1;
    if (ioctlsocket(socketHandle, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender nonblocking configuration failed", WSAGetLastError()));
    }
    sockaddr_storage localAddress{};
    int localLength = 0;
    auto converted = fillSockaddr(
        request.localEndpoint(), localAddress, localLength);
    if (!converted) return fail(converted.error());
    if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&localAddress),
             localLength) == SOCKET_ERROR) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender bind failed", WSAGetLastError()));
    }
    sockaddr_storage boundAddress{};
    int boundLength = sizeof(boundAddress);
    if (getsockname(socketHandle,
                    reinterpret_cast<sockaddr*>(&boundAddress),
                    &boundLength) == SOCKET_ERROR) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender getsockname failed", WSAGetLastError()));
    }
    auto boundEndpoint = endpointFromSockaddr(boundAddress);
    if (!boundEndpoint) return fail(boundEndpoint.error());
    sockaddr_storage remoteAddress{};
    int remoteLength = 0;
    converted = fillSockaddr(
        request.remoteEndpoint(), remoteAddress, remoteLength);
    if (!converted) return fail(converted.error());

    m_impl->socket = socketHandle;
    m_impl->remoteAddress = remoteAddress;
    m_impl->remoteAddressLength = remoteLength;
    m_impl->localEndpoint = std::move(boundEndpoint.value());
    m_impl->remoteEndpoint = request.remoteEndpoint();
    m_impl->maximumDatagramBytes = request.maximumDatagramBytes();
    return ::media::Status::success();
#else
    (void)request;
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "UDP sender socket is currently implemented for Windows"));
#endif
}

MediaUdpDatagramSendOutcome MediaUdpDatagramSenderSocket::send(
    std::span<const std::uint8_t> datagram)
{
#ifdef _WIN32
    if (m_impl->socket == INVALID_SOCKET || datagram.empty() ||
        datagram.size() > m_impl->maximumDatagramBytes ||
        datagram.size() >
            static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return MediaUdpDatagramSendOutcome::notAccepted(
            ::media::ErrorInfo::invalidArgument(
                "invalid UDP sender socket send request"));
    }
    const int sent = sendto(
        m_impl->socket,
        reinterpret_cast<const char*>(datagram.data()),
        static_cast<int>(datagram.size()),
        0,
        reinterpret_cast<const sockaddr*>(&m_impl->remoteAddress),
        m_impl->remoteAddressLength);
    if (sent == SOCKET_ERROR) {
        return MediaUdpDatagramSendOutcome::notAccepted(
            ::media::ErrorInfo::ioFailure(
                "UDP sendto accepted no bytes", WSAGetLastError()));
    }
    if (sent == static_cast<int>(datagram.size())) {
        return MediaUdpDatagramSendOutcome::accepted(datagram.size());
    }
    if (sent <= 0) {
        return MediaUdpDatagramSendOutcome::notAccepted(
            ::media::ErrorInfo::ioFailure(
                "UDP sendto accepted no bytes"));
    }
    return MediaUdpDatagramSendOutcome::ambiguousPartial(
        ::media::ErrorInfo::ioFailure(
            "UDP sendto reported a positive short send"),
        static_cast<std::size_t>(sent));
#else
    (void)datagram;
    return MediaUdpDatagramSendOutcome::notAccepted(
        ::media::ErrorInfo::unsupported(
            "UDP sender socket is currently implemented for Windows"));
#endif
}

std::optional<MediaUdpDatagramEndpoint>
MediaUdpDatagramSenderSocket::localEndpoint() const
{
    return m_impl->localEndpoint;
}

std::optional<MediaUdpDatagramEndpoint>
MediaUdpDatagramSenderSocket::remoteEndpoint() const
{
    return m_impl->remoteEndpoint;
}

void MediaUdpDatagramSenderSocket::close() noexcept
{
#ifdef _WIN32
    if (m_impl->socket != INVALID_SOCKET) {
        closesocket(m_impl->socket);
        m_impl->socket = INVALID_SOCKET;
    }
#endif
    m_impl->localEndpoint.reset();
    m_impl->remoteEndpoint.reset();
    m_impl->maximumDatagramBytes = 0;
}

MediaUdpDatagramSenderSocketFactory::MediaUdpDatagramSenderSocketFactory(
    std::shared_ptr<MediaSocketRuntime> runtime)
    : m_runtime(std::move(runtime))
{
}

::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>
MediaUdpDatagramSenderSocketFactory::create()
{
    if (!m_runtime) {
        return ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>::failure(
            ::media::ErrorInfo::notInitialized(
                "UDP sender socket factory requires a socket runtime"));
    }
    try {
        std::unique_ptr<MediaUdpDatagramSenderPort> port =
            std::make_unique<MediaUdpDatagramSenderSocket>(m_runtime);
        return ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>::success(
            std::move(port));
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaUdpDatagramSenderSocket"));
    }
}

} // namespace media::ffmpeg::graph

#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
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
#else
    int socket = -1;
    sockaddr_storage remoteAddress{};
    socklen_t remoteAddressLength = 0;
#endif
    std::optional<MediaUdpDatagramEndpoint> localEndpoint;
    std::optional<MediaUdpDatagramEndpoint> remoteEndpoint;
    std::size_t maximumDatagramBytes = 0;
    bool openAttempted = false;
};

namespace {

::media::Status fillSockaddr(const MediaUdpDatagramEndpoint& endpoint,
                             sockaddr_storage& storage,
#ifdef _WIN32
                             int& length)
#else
                             socklen_t& length)
#endif
{
    std::memset(&storage, 0, sizeof(storage));
    if (endpoint.addressFamily() == MediaIpAddressFamily::Ipv4) {
        auto* address = reinterpret_cast<sockaddr_in*>(&storage);
        address->sin_family = AF_INET;
        address->sin_port = htons(endpoint.port());
#ifdef _WIN32
        const int converted = InetPtonA(
            AF_INET, endpoint.numericAddress().c_str(), &address->sin_addr);
#else
        const int converted = inet_pton(
            AF_INET, endpoint.numericAddress().c_str(), &address->sin_addr);
#endif
        if (converted != 1) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "invalid numeric IPv4 UDP sender address"));
        }
        length = sizeof(sockaddr_in);
        return ::media::Status::success();
    }
    auto* address = reinterpret_cast<sockaddr_in6*>(&storage);
    address->sin6_family = AF_INET6;
    address->sin6_port = htons(endpoint.port());
#ifdef _WIN32
    const int converted = InetPtonA(
        AF_INET6, endpoint.numericAddress().c_str(), &address->sin6_addr);
#else
    const int converted = inet_pton(
        AF_INET6, endpoint.numericAddress().c_str(), &address->sin6_addr);
#endif
    if (converted != 1) {
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
#ifdef _WIN32
        if (!InetNtopA(AF_INET, const_cast<IN_ADDR*>(&address->sin_addr),
                       addressText.data(), static_cast<DWORD>(addressText.size()))) {
#else
        if (!inet_ntop(AF_INET, &address->sin_addr,
                       addressText.data(), addressText.size())) {
#endif
            return ::media::Result<MediaUdpDatagramEndpoint>::failure(
                ::media::ErrorInfo::ioFailure(
                    "InetNtop failed for bound IPv4 sender endpoint",
#ifdef _WIN32
                    WSAGetLastError()));
#else
                    errno));
#endif
        }
        return MediaUdpDatagramEndpoint::create(
            MediaIpAddressFamily::Ipv4, addressText.data(), ntohs(address->sin_port));
    }
    if (storage.ss_family == AF_INET6) {
        const auto* address = reinterpret_cast<const sockaddr_in6*>(&storage);
#ifdef _WIN32
        if (!InetNtopA(AF_INET6, const_cast<IN6_ADDR*>(&address->sin6_addr),
                       addressText.data(), static_cast<DWORD>(addressText.size()))) {
#else
        if (!inet_ntop(AF_INET6, &address->sin6_addr,
                       addressText.data(), addressText.size())) {
#endif
            return ::media::Result<MediaUdpDatagramEndpoint>::failure(
                ::media::ErrorInfo::ioFailure(
                    "InetNtop failed for bound IPv6 sender endpoint",
#ifdef _WIN32
                    WSAGetLastError()));
#else
                    errno));
#endif
        }
        return MediaUdpDatagramEndpoint::create(
            MediaIpAddressFamily::Ipv6, addressText.data(), ntohs(address->sin6_port));
    }
    return ::media::Result<MediaUdpDatagramEndpoint>::failure(
        ::media::ErrorInfo::internalError(
            "bound UDP sender endpoint has an unknown address family"));
}

} // namespace

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
 #ifdef _WIN32
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
#else
    int socketHandle = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle < 0) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "UDP sender socket creation failed", errno));
    }
    const auto fail = [&socketHandle](::media::ErrorInfo error) {
        ::close(socketHandle);
        socketHandle = -1;
        return ::media::Status::failure(std::move(error));
    };
#endif
#ifdef _WIN32
    const BOOL exclusiveAddressUse = TRUE;
    if (setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusiveAddressUse),
                   sizeof(exclusiveAddressUse)) == SOCKET_ERROR) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender SO_EXCLUSIVEADDRUSE configuration failed",
            WSAGetLastError()));
    }
#endif
    const int sendBufferBytes = request.sendBufferBytes();
    if (setsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF,
#ifdef _WIN32
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
#else
                   &sendBufferBytes, sizeof(sendBufferBytes)) != 0) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender SO_SNDBUF configuration failed", errno));
    }
    const int currentFlags = fcntl(socketHandle, F_GETFL, 0);
    if (currentFlags < 0 ||
        fcntl(socketHandle, F_SETFL, currentFlags | O_NONBLOCK) != 0) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender nonblocking configuration failed", errno));
    }
#endif
    sockaddr_storage localAddress{};
#ifdef _WIN32
    int localLength = 0;
#else
    socklen_t localLength = 0;
#endif
    auto converted = fillSockaddr(
        request.localEndpoint(), localAddress, localLength);
    if (!converted) return fail(converted.error());
    if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&localAddress),
             localLength)
#ifdef _WIN32
            == SOCKET_ERROR
#else
            != 0
#endif
    ) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender bind failed",
#ifdef _WIN32
            WSAGetLastError()));
#else
            errno));
#endif
    }
    sockaddr_storage boundAddress{};
#ifdef _WIN32
    int boundLength = sizeof(boundAddress);
#else
    socklen_t boundLength = sizeof(boundAddress);
#endif
    if (getsockname(socketHandle,
                    reinterpret_cast<sockaddr*>(&boundAddress),
                    &boundLength)
#ifdef _WIN32
            == SOCKET_ERROR
#else
            != 0
#endif
    ) {
        return fail(::media::ErrorInfo::ioFailure(
            "UDP sender getsockname failed",
#ifdef _WIN32
            WSAGetLastError()));
#else
            errno));
#endif
    }
    auto boundEndpoint = endpointFromSockaddr(boundAddress);
    if (!boundEndpoint) return fail(boundEndpoint.error());
    sockaddr_storage remoteAddress{};
#ifdef _WIN32
    int remoteLength = 0;
#else
    socklen_t remoteLength = 0;
#endif
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
}

MediaUdpDatagramSendOutcome MediaUdpDatagramSenderSocket::send(
    std::span<const std::uint8_t> datagram)
{
    if (
#ifdef _WIN32
        m_impl->socket == INVALID_SOCKET ||
#else
        m_impl->socket < 0 ||
#endif
        datagram.empty() ||
        datagram.size() > m_impl->maximumDatagramBytes ||
        datagram.size() >
            static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return MediaUdpDatagramSendOutcome::notAccepted(
            ::media::ErrorInfo::invalidArgument(
                "invalid UDP sender socket send request"));
    }
    const auto sent = sendto(
        m_impl->socket,
#ifdef _WIN32
        reinterpret_cast<const char*>(datagram.data()),
        static_cast<int>(datagram.size()),
#else
        datagram.data(), datagram.size(),
#endif
        0,
        reinterpret_cast<const sockaddr*>(&m_impl->remoteAddress),
        m_impl->remoteAddressLength);
    if (
#ifdef _WIN32
        sent == SOCKET_ERROR
#else
        sent < 0
#endif
    ) {
#ifdef _WIN32
        const int nativeError = WSAGetLastError();
        if (nativeError == WSAEWOULDBLOCK || nativeError == WSAENOBUFS) {
            return MediaUdpDatagramSendOutcome::notAccepted(
                ::media::ErrorInfo::make(
                    ::media::ErrorCode::WouldBlock,
                    "UDP send buffer rejected datagram under pressure",
                    nativeError));
        }
#else
        const int nativeError = errno;
        if (nativeError == EAGAIN || nativeError == EWOULDBLOCK ||
            nativeError == ENOBUFS) {
            return MediaUdpDatagramSendOutcome::notAccepted(
                ::media::ErrorInfo::make(
                    ::media::ErrorCode::WouldBlock,
                    "UDP send buffer rejected datagram under pressure",
                    nativeError));
        }
#endif
        return MediaUdpDatagramSendOutcome::notAccepted(
            ::media::ErrorInfo::ioFailure(
                "UDP sendto accepted no bytes",
#ifdef _WIN32
                nativeError));
#else
                nativeError));
#endif
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
#else
    if (m_impl->socket >= 0) {
        ::close(m_impl->socket);
        m_impl->socket = -1;
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

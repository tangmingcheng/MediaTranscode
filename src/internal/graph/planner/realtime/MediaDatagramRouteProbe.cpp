#include "internal/graph/planner/realtime/MediaDatagramRouteProbe.h"

#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;

struct RemoteEndpointFact final {
    MediaIpAddressFamily addressFamily;
    std::string numericAddress;
    std::uint16_t port;
};

class NativeSocket final {
public:
#ifdef _WIN32
    using Handle = SOCKET;
    static constexpr Handle Invalid = INVALID_SOCKET;
#else
    using Handle = int;
    static constexpr Handle Invalid = -1;
#endif

    explicit NativeSocket(Handle handle = Invalid) noexcept
        : m_handle(handle)
    {
    }

    ~NativeSocket()
    {
        if (m_handle == Invalid) {
            return;
        }
#ifdef _WIN32
        closesocket(m_handle);
#else
        close(m_handle);
#endif
    }

    NativeSocket(const NativeSocket&) = delete;
    NativeSocket& operator=(const NativeSocket&) = delete;

    Handle get() const noexcept { return m_handle; }

private:
    Handle m_handle;
};

#ifdef _WIN32
class WinsockSession final {
public:
    WinsockSession() noexcept
    {
        m_error = WSAStartup(MAKEWORD(2, 2), &m_data);
    }

    ~WinsockSession()
    {
        if (m_error == 0) {
            WSACleanup();
        }
    }

    int error() const noexcept { return m_error; }

private:
    WSADATA m_data{};
    int m_error = 0;
};
#endif

::media::Result<RemoteEndpointFact> remoteEndpoint(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    std::string address;
    std::uint16_t port = 0;
    if (request.output.transport == MediaOutputTransportKind::RtpAvp) {
        if (!request.output.basePort || *request.output.basePort > 65'535) {
            return ::media::Result<RemoteEndpointFact>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "realtime egress route requires a valid RTP endpoint"));
        }
        address = request.output.host;
        port = static_cast<std::uint16_t>(*request.output.basePort);
    } else {
        auto endpoint = parseRtpUdpUrlEndpoint(request.output.url);
        if (!endpoint || endpoint.value().scheme != "udp") {
            return ::media::Result<RemoteEndpointFact>::failure(
                endpoint
                    ? ::media::ErrorInfo::invalidArgument(
                          "realtime egress route requires a UDP endpoint")
                    : endpoint.error());
        }
        address = endpoint.value().host;
        port = endpoint.value().port;
    }
    if (address.size() > 2 && address.front() == '[' &&
        address.back() == ']') {
        address = address.substr(1, address.size() - 2);
    }
    if (MediaNumericIpAddress::create(MediaIpAddressFamily::Ipv4, address)) {
        return ::media::Result<RemoteEndpointFact>::success(
            {MediaIpAddressFamily::Ipv4, std::move(address), port});
    }
    if (MediaNumericIpAddress::create(MediaIpAddressFamily::Ipv6, address)) {
        return ::media::Result<RemoteEndpointFact>::success(
            {MediaIpAddressFamily::Ipv6, std::move(address), port});
    }
    return ::media::Result<RemoteEndpointFact>::failure(
        ::media::ErrorInfo::invalidArgument(
            "realtime egress route requires a numeric destination address"));
}

} // namespace

::media::Result<MediaDatagramRouteFact> MediaDatagramRouteProbe::probe(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    auto remote = remoteEndpoint(request);
    if (!remote) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            remote.error());
    }
#ifdef _WIN32
    WinsockSession winsock;
    if (winsock.error() != 0) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route Winsock initialization failed", winsock.error()));
    }
#endif
    const int nativeFamily =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? AF_INET
            : AF_INET6;
    NativeSocket socketHandle(
        ::socket(nativeFamily, SOCK_DGRAM, IPPROTO_UDP));
    if (socketHandle.get() == NativeSocket::Invalid) {
#ifdef _WIN32
        const int native = WSAGetLastError();
#else
        const int native = errno;
#endif
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route probe socket creation failed", native));
    }

    sockaddr_storage destination{};
    socklen_t destinationLength = 0;
    if (remote.value().addressFamily == MediaIpAddressFamily::Ipv4) {
        auto* address = reinterpret_cast<sockaddr_in*>(&destination);
        address->sin_family = AF_INET;
        address->sin_port = htons(remote.value().port);
        destinationLength = sizeof(*address);
        if (inet_pton(AF_INET, remote.value().numericAddress.c_str(),
                      &address->sin_addr) != 1) {
            return ::media::Result<MediaDatagramRouteFact>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "invalid IPv4 egress endpoint"));
        }
    } else {
        auto* address = reinterpret_cast<sockaddr_in6*>(&destination);
        address->sin6_family = AF_INET6;
        address->sin6_port = htons(remote.value().port);
        destinationLength = sizeof(*address);
        if (inet_pton(AF_INET6, remote.value().numericAddress.c_str(),
                      &address->sin6_addr) != 1) {
            return ::media::Result<MediaDatagramRouteFact>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "invalid IPv6 egress endpoint"));
        }
    }
    if (::connect(socketHandle.get(),
                  reinterpret_cast<const sockaddr*>(&destination),
                  destinationLength) != 0) {
#ifdef _WIN32
        const int native = WSAGetLastError();
#else
        const int native = errno;
#endif
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route probe connect failed", native));
    }

    sockaddr_storage local{};
#ifdef _WIN32
    int localLength = sizeof(local);
#else
    socklen_t localLength = sizeof(local);
#endif
    if (getsockname(socketHandle.get(), reinterpret_cast<sockaddr*>(&local),
                    &localLength) != 0) {
#ifdef _WIN32
        const int native = WSAGetLastError();
#else
        const int native = errno;
#endif
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route local address readback failed", native));
    }
    char presentation[INET6_ADDRSTRLEN]{};
    const void* localBytes =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? static_cast<const void*>(
                  &reinterpret_cast<sockaddr_in*>(&local)->sin_addr)
            : static_cast<const void*>(
                  &reinterpret_cast<sockaddr_in6*>(&local)->sin6_addr);
    if (!inet_ntop(nativeFamily, localBytes, presentation,
                   sizeof(presentation))) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route local address formatting failed"));
    }

    std::uint64_t maximumIpPacketBytes = 0;
    std::string serviceScopeId;
    std::string authority;
#ifdef _WIN32
    HMODULE library = LoadLibraryExW(
        L"iphlpapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route interface API load failed",
                static_cast<int>(GetLastError())));
    }
    using GetBestRoute2Function = NETIO_STATUS(WINAPI*)(
        const NET_LUID*, NET_IFINDEX, const SOCKADDR_INET*,
        const SOCKADDR_INET*, ULONG, PMIB_IPFORWARD_ROW2, SOCKADDR_INET*);
    using GetIfEntry2Function = NETIO_STATUS(WINAPI*)(PMIB_IF_ROW2);
    const auto getBestRoute2 = reinterpret_cast<GetBestRoute2Function>(
        GetProcAddress(library, "GetBestRoute2"));
    const auto getIfEntry2 = reinterpret_cast<GetIfEntry2Function>(
        GetProcAddress(library, "GetIfEntry2"));
    MIB_IPFORWARD_ROW2 bestRoute{};
    SOCKADDR_INET bestSource{};
    MIB_IF_ROW2 row{};
    const DWORD routeStatus = getBestRoute2
        ? getBestRoute2(
              nullptr, 0, nullptr,
              reinterpret_cast<const SOCKADDR_INET*>(&destination), 0,
              &bestRoute, &bestSource)
        : ERROR_PROC_NOT_FOUND;
    row.InterfaceLuid = bestRoute.InterfaceLuid;
    const DWORD rowStatus = routeStatus == NO_ERROR && getIfEntry2
        ? getIfEntry2(&row)
        : ERROR_PROC_NOT_FOUND;
    FreeLibrary(library);
    if (routeStatus != NO_ERROR || rowStatus != NO_ERROR || row.Mtu == 0) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route interface MTU readback failed",
                static_cast<int>(routeStatus != NO_ERROR
                                     ? routeStatus
                                     : rowStatus)));
    }
    maximumIpPacketBytes = row.Mtu;
    serviceScopeId = "ifindex:" + std::to_string(row.InterfaceIndex);
    authority = "connected-udp-source+GetBestRoute2+GetIfEntry2";
#else
    int nativeMtu = 0;
    socklen_t mtuLength = sizeof(nativeMtu);
    const int level =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? IPPROTO_IP
            : IPPROTO_IPV6;
    const int option =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? IP_MTU
            : IPV6_MTU;
    if (getsockopt(socketHandle.get(), level, option, &nativeMtu,
                   &mtuLength) != 0 ||
        nativeMtu <= 0) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress connected-route MTU readback failed", errno));
    }
    maximumIpPacketBytes = static_cast<std::uint64_t>(nativeMtu);
    serviceScopeId = "route:" + std::string(presentation);
    authority = "connected-udp-IP_MTU-readback";
#endif
    const std::uint64_t protocolMaximum =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? 65'535U
            : 65'575U;
    maximumIpPacketBytes =
        (std::min)(maximumIpPacketBytes, protocolMaximum);
    const auto headerBytes =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? Ipv4HeaderBytes
            : Ipv6HeaderBytes;
    if (maximumIpPacketBytes <= headerBytes + UdpHeaderBytes) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::unsupported(
                "egress route MTU cannot carry a UDP payload"));
    }
    return ::media::Result<MediaDatagramRouteFact>::success({
        remote.value().addressFamily,
        presentation,
        maximumIpPacketBytes,
        std::move(serviceScopeId),
        std::move(authority)});
}

} // namespace media::ffmpeg::graph

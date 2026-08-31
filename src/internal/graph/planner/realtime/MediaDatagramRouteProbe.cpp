#include "internal/graph/planner/realtime/MediaDatagramRouteProbe.h"

#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
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
#include <ifaddrs.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
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

#ifndef _WIN32
struct LocalInterfaceFact final {
    unsigned int index;
    std::uint64_t maximumIpPacketBytes;
    bool loopback;
};

bool sameIpAddress(
    MediaIpAddressFamily family,
    const sockaddr* candidate,
    const sockaddr_storage& expected)
{
    if (!candidate) return false;
    const int nativeFamily = family == MediaIpAddressFamily::Ipv4
        ? AF_INET
        : AF_INET6;
    if (candidate->sa_family != nativeFamily) return false;
    return nativeFamily == AF_INET
        ? std::memcmp(
              &reinterpret_cast<const sockaddr_in*>(candidate)->sin_addr,
              &reinterpret_cast<const sockaddr_in*>(&expected)->sin_addr,
              sizeof(in_addr)) == 0
        : std::memcmp(
              &reinterpret_cast<const sockaddr_in6*>(candidate)->sin6_addr,
              &reinterpret_cast<const sockaddr_in6*>(&expected)->sin6_addr,
              sizeof(in6_addr)) == 0;
}

::media::Result<unsigned int> uniqueSourceInterfaceIndex(
    MediaIpAddressFamily family,
    const sockaddr_storage& local)
{
    ifaddrs* rawInterfaces = nullptr;
    if (getifaddrs(&rawInterfaces) != 0) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress source interface enumeration failed", errno));
    }
    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> interfaces(
        rawInterfaces, &freeifaddrs);
    unsigned int matchedIndex = 0;
    for (auto* candidate = interfaces.get(); candidate != nullptr;
         candidate = candidate->ifa_next) {
        if (!sameIpAddress(family, candidate->ifa_addr, local)) continue;
        const unsigned int index = if_nametoindex(candidate->ifa_name);
        if (index == 0) {
            return ::media::Result<unsigned int>::failure(
                ::media::ErrorInfo::ioFailure(
                    "egress source interface index readback failed", errno));
        }
        if (matchedIndex != 0 && matchedIndex != index) {
            return ::media::Result<unsigned int>::failure(
                ::media::ErrorInfo::unsupported(
                    "egress connected source address belongs to multiple interfaces"));
        }
        matchedIndex = index;
    }
    if (matchedIndex == 0) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress connected source address has no local interface"));
    }
    return ::media::Result<unsigned int>::success(matchedIndex);
}

::media::Result<unsigned int> connectedRouteInterfaceIndex(
    NativeSocket::Handle socketHandle,
    MediaIpAddressFamily family,
    const sockaddr_storage& local,
    const sockaddr_storage& destination)
{
    NativeSocket routeSocket(::socket(
        AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE));
    if (routeSocket.get() == NativeSocket::Invalid) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress kernel route query socket creation failed", errno));
    }
    sockaddr_nl localNetlink{};
    localNetlink.nl_family = AF_NETLINK;
    if (::bind(routeSocket.get(),
               reinterpret_cast<const sockaddr*>(&localNetlink),
               sizeof(localNetlink)) != 0) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress kernel route query bind failed", errno));
    }

    std::array<std::byte, 512> requestBytes{};
    auto* header = reinterpret_cast<nlmsghdr*>(requestBytes.data());
    header->nlmsg_len = NLMSG_LENGTH(sizeof(rtmsg));
    header->nlmsg_type = RTM_GETROUTE;
    header->nlmsg_flags = NLM_F_REQUEST;
    header->nlmsg_seq = 1;
    auto* route = reinterpret_cast<rtmsg*>(NLMSG_DATA(header));
    route->rtm_family = family == MediaIpAddressFamily::Ipv4
        ? AF_INET
        : AF_INET6;
    route->rtm_dst_len = family == MediaIpAddressFamily::Ipv4 ? 32 : 128;
    route->rtm_src_len = route->rtm_dst_len;

    const auto appendAttribute = [&](std::uint16_t type, const void* data,
                                     std::size_t size) {
        const auto offset = NLMSG_ALIGN(header->nlmsg_len);
        const auto attributeLength = RTA_LENGTH(size);
        const auto required = offset + RTA_ALIGN(attributeLength);
        if (required > requestBytes.size()) return false;
        auto* attribute = reinterpret_cast<rtattr*>(
            requestBytes.data() + offset);
        attribute->rta_type = type;
        attribute->rta_len = static_cast<unsigned short>(attributeLength);
        std::memcpy(RTA_DATA(attribute), data, size);
        header->nlmsg_len = static_cast<unsigned int>(required);
        return true;
    };
    const void* destinationBytes = family == MediaIpAddressFamily::Ipv4
        ? static_cast<const void*>(
              &reinterpret_cast<const sockaddr_in*>(&destination)->sin_addr)
        : static_cast<const void*>(
              &reinterpret_cast<const sockaddr_in6*>(&destination)->sin6_addr);
    const void* sourceBytes = family == MediaIpAddressFamily::Ipv4
        ? static_cast<const void*>(
              &reinterpret_cast<const sockaddr_in*>(&local)->sin_addr)
        : static_cast<const void*>(
              &reinterpret_cast<const sockaddr_in6*>(&local)->sin6_addr);
    const std::size_t addressBytes = family == MediaIpAddressFamily::Ipv4
        ? sizeof(in_addr)
        : sizeof(in6_addr);
    const uid_t uid = geteuid();
    int socketMark = 0;
    socklen_t markLength = sizeof(socketMark);
    const bool hasMark = getsockopt(socketHandle, SOL_SOCKET, SO_MARK,
                                    &socketMark, &markLength) == 0 &&
        socketMark != 0;
    if (!appendAttribute(RTA_DST, destinationBytes, addressBytes) ||
        !appendAttribute(RTA_SRC, sourceBytes, addressBytes) ||
        !appendAttribute(RTA_UID, &uid, sizeof(uid)) ||
        (hasMark && !appendAttribute(RTA_MARK, &socketMark,
                                     sizeof(socketMark)))) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "egress kernel route query attributes exceed request capacity"));
    }

    sockaddr_nl kernel{};
    kernel.nl_family = AF_NETLINK;
    if (::sendto(routeSocket.get(), requestBytes.data(), header->nlmsg_len, 0,
                 reinterpret_cast<const sockaddr*>(&kernel),
                 sizeof(kernel)) < 0) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress kernel route query send failed", errno));
    }
    std::array<std::byte, 8192> response{};
    const auto received = ::recv(
        routeSocket.get(), response.data(), response.size(), 0);
    if (received < 0) {
        return ::media::Result<unsigned int>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress kernel route query receive failed", errno));
    }
    auto remaining = static_cast<unsigned int>(received);
    for (auto* message = reinterpret_cast<nlmsghdr*>(response.data());
         NLMSG_OK(message, remaining);
         message = NLMSG_NEXT(message, remaining)) {
        if (message->nlmsg_seq != header->nlmsg_seq) continue;
        if (message->nlmsg_type == NLMSG_ERROR) {
            const auto* error = reinterpret_cast<const nlmsgerr*>(
                NLMSG_DATA(message));
            return ::media::Result<unsigned int>::failure(
                ::media::ErrorInfo::ioFailure(
                    "egress kernel route query failed",
                    error->error < 0 ? -error->error : error->error));
        }
        if (message->nlmsg_type != RTM_NEWROUTE) continue;
        const auto* resultRoute = reinterpret_cast<const rtmsg*>(
            NLMSG_DATA(message));
        auto attributeBytes = RTM_PAYLOAD(message);
        for (auto* attribute = RTM_RTA(resultRoute);
             RTA_OK(attribute, attributeBytes);
             attribute = RTA_NEXT(attribute, attributeBytes)) {
            if (attribute->rta_type != RTA_OIF ||
                RTA_PAYLOAD(attribute) < sizeof(unsigned int)) {
                continue;
            }
            const auto index = *reinterpret_cast<const unsigned int*>(
                RTA_DATA(attribute));
            if (index != 0) {
                return ::media::Result<unsigned int>::success(index);
            }
        }
    }
    return ::media::Result<unsigned int>::failure(
        ::media::ErrorInfo::ioFailure(
            "egress kernel route query returned no output interface"));
}

::media::Result<LocalInterfaceFact> localInterfaceFact(
    NativeSocket::Handle socketHandle,
    unsigned int index)
{
    char name[IF_NAMESIZE]{};
    if (!if_indextoname(index, name)) {
        return ::media::Result<LocalInterfaceFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route interface name readback failed", errno));
    }
    ifreq request{};
    std::strncpy(request.ifr_name, name, sizeof(request.ifr_name) - 1U);
    if (ioctl(socketHandle, SIOCGIFMTU, &request) != 0 ||
        request.ifr_mtu <= 0) {
        return ::media::Result<LocalInterfaceFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route interface MTU readback failed", errno));
    }
    const auto maximumIpPacketBytes =
        static_cast<std::uint64_t>(request.ifr_mtu);
    ifreq flagRequest{};
    std::strncpy(flagRequest.ifr_name, name,
                 sizeof(flagRequest.ifr_name) - 1U);
    if (ioctl(socketHandle, SIOCGIFFLAGS, &flagRequest) != 0) {
        return ::media::Result<LocalInterfaceFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route interface flags readback failed", errno));
    }
    return ::media::Result<LocalInterfaceFact>::success(
        {index, maximumIpPacketBytes,
         (flagRequest.ifr_flags & IFF_LOOPBACK) != 0});
}

::media::Result<LocalInterfaceFact> selectedInterfaceFact(
    NativeSocket::Handle socketHandle,
    MediaIpAddressFamily family,
    const sockaddr_storage& local,
    const sockaddr_storage& destination)
{
    auto routeIndex = connectedRouteInterfaceIndex(
        socketHandle, family, local, destination);
    if (!routeIndex) {
        return ::media::Result<LocalInterfaceFact>::failure(
            routeIndex.error());
    }
    auto routeInterface = localInterfaceFact(
        socketHandle, routeIndex.value());
    if (!routeInterface) return routeInterface;

    if (!routeInterface.value().loopback) return routeInterface;

    auto sourceIndex = uniqueSourceInterfaceIndex(family, local);
    if (!sourceIndex) {
        return ::media::Result<LocalInterfaceFact>::failure(
            sourceIndex.error());
    }
    return sourceIndex.value() == routeIndex.value()
        ? routeInterface
        : localInterfaceFact(socketHandle, sourceIndex.value());
}
#endif

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
    DWORD nativeMtu = 0;
    int mtuLength = sizeof(nativeMtu);
    const int mtuLevel =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? IPPROTO_IP
            : IPPROTO_IPV6;
    const int mtuOption =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
            ? IP_MTU
            : IPV6_MTU;
    if (getsockopt(socketHandle.get(), mtuLevel, mtuOption,
                   reinterpret_cast<char*>(&nativeMtu), &mtuLength) != 0 ||
        nativeMtu == 0) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress connected-route path MTU readback failed",
                WSAGetLastError()));
    }
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
    const auto* selectedSource =
        reinterpret_cast<const SOCKADDR_INET*>(&local);
    const DWORD routeStatus = getBestRoute2
        ? getBestRoute2(
              nullptr, 0, selectedSource,
              reinterpret_cast<const SOCKADDR_INET*>(&destination), 0,
              &bestRoute, &bestSource)
        : ERROR_PROC_NOT_FOUND;
    row.InterfaceLuid = bestRoute.InterfaceLuid;
    const DWORD rowStatus = routeStatus == NO_ERROR && getIfEntry2
        ? getIfEntry2(&row)
        : ERROR_PROC_NOT_FOUND;
    FreeLibrary(library);
    const bool sourceMatches =
        remote.value().addressFamily == MediaIpAddressFamily::Ipv4
        ? bestSource.Ipv4.sin_addr.S_un.S_addr ==
              selectedSource->Ipv4.sin_addr.S_un.S_addr
        : std::memcmp(&bestSource.Ipv6.sin6_addr,
                      &selectedSource->Ipv6.sin6_addr,
                      sizeof(in6_addr)) == 0 &&
              bestSource.Ipv6.sin6_scope_id ==
                  selectedSource->Ipv6.sin6_scope_id;
    if (routeStatus != NO_ERROR || rowStatus != NO_ERROR ||
        !sourceMatches || bestRoute.InterfaceIndex == 0 ||
        row.InterfaceIndex != bestRoute.InterfaceIndex || row.Mtu == 0) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            ::media::ErrorInfo::ioFailure(
                "egress route interface MTU readback failed",
                static_cast<int>(routeStatus != NO_ERROR
                                     ? routeStatus
                                     : rowStatus)));
    }
    maximumIpPacketBytes = (std::min)(
        static_cast<std::uint64_t>(nativeMtu),
        static_cast<std::uint64_t>(row.Mtu));
    serviceScopeId = "ifindex:" + std::to_string(row.InterfaceIndex);
    authority =
        "connected-udp-system-path-mtu+GetBestRoute2+GetIfEntry2";
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
    auto interface = selectedInterfaceFact(
        socketHandle.get(), remote.value().addressFamily, local,
        destination);
    if (!interface) {
        return ::media::Result<MediaDatagramRouteFact>::failure(
            interface.error());
    }
    maximumIpPacketBytes = (std::min)(
        static_cast<std::uint64_t>(nativeMtu),
        interface.value().maximumIpPacketBytes);
    serviceScopeId = "ifindex:" + std::to_string(interface.value().index);
    authority =
        "connected-udp-system-path-mtu+selected-source-interface-mtu";
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

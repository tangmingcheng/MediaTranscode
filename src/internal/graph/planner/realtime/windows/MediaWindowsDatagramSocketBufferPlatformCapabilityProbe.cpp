#include "internal/graph/planner/realtime/MediaDatagramSocketBufferPlanner.h"
#include "internal/graph/planner/realtime/windows/MediaWindowsSocketProbeHandle.h"

#ifdef _WIN32

#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaDatagramSocketBufferPlatformCapability>
MediaDatagramSocketBufferPlatformCapabilityProbe::scan(
    std::uint64_t minimumRequiredEffectiveBytes,
    MediaIpAddressFamily addressFamily) noexcept
{
    using Result =
        ::media::Result<MediaDatagramSocketBufferPlatformCapability>;
    if (minimumRequiredEffectiveBytes == 0 ||
        minimumRequiredEffectiveBytes > static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Windows SO_SNDBUF probe target is outside the integer range"));
    }
    MediaWindowsWinsockProbeSession session;
    if (session.error() != 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Windows SO_SNDBUF probe Winsock initialization failed",
            session.error()));
    }
    int nativeFamily = AF_UNSPEC;
    switch (addressFamily) {
    case MediaIpAddressFamily::Ipv4:
        nativeFamily = AF_INET;
        break;
    case MediaIpAddressFamily::Ipv6:
        nativeFamily = AF_INET6;
        break;
    }
    if (nativeFamily == AF_UNSPEC) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Windows SO_SNDBUF probe address family is unsupported"));
    }
    MediaWindowsSocketProbeHandle socketHandle(
        WSASocketW(nativeFamily, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, 0));
    if (socketHandle.get() == INVALID_SOCKET) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Windows SO_SNDBUF probe socket creation failed",
            WSAGetLastError()));
    }
    const int apiRequestedBytes =
        static_cast<int>(minimumRequiredEffectiveBytes);
    if (setsockopt(socketHandle.get(), SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<const char*>(&apiRequestedBytes),
                   sizeof(apiRequestedBytes)) == SOCKET_ERROR) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Windows SO_SNDBUF probe set failed", WSAGetLastError()));
    }
    int effectiveBytes = 0;
    int optionLength = sizeof(effectiveBytes);
    if (getsockopt(socketHandle.get(), SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<char*>(&effectiveBytes),
                   &optionLength) == SOCKET_ERROR) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Windows SO_SNDBUF probe read failed", WSAGetLastError()));
    }
    if (optionLength != sizeof(effectiveBytes) || effectiveBytes <= 0 ||
        effectiveBytes != apiRequestedBytes) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Winsock provider cannot prove exact SO_SNDBUF target mapping"));
    }
    const auto verifiedBytes = static_cast<std::uint64_t>(effectiveBytes);
    return Result::success({
        MediaDatagramSocketBufferAccounting::Exact,
        verifiedBytes,
        verifiedBytes,
        verifiedBytes,
        "Winsock provider per-target SO_SNDBUF set/get exact readback"});
}

} // namespace media::ffmpeg::graph

#endif

#include "internal/graph/planner/realtime/MediaDatagramSocketBufferPlanner.h"
#include "internal/graph/planner/realtime/linux/MediaLinuxSocketProbeHandle.h"

#ifdef __linux__

#include <cerrno>
#include <cstdint>
#include <limits>
#include <sys/socket.h>

#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> effectiveSendBuffer(
    int socketHandle,
    int apiRequestedBytes) noexcept
{
    using Result = ::media::Result<std::uint64_t>;
    if (::setsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF,
                     &apiRequestedBytes, sizeof(apiRequestedBytes)) != 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Linux SO_SNDBUF capability probe set failed", errno));
    }
    int effectiveBytes = 0;
    socklen_t size = sizeof(effectiveBytes);
    if (::getsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF,
                     &effectiveBytes, &size) != 0 ||
        size != sizeof(effectiveBytes) || effectiveBytes <= 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Linux SO_SNDBUF capability probe read failed", errno));
    }
    return Result::success(static_cast<std::uint64_t>(effectiveBytes));
}

} // namespace

::media::Result<MediaDatagramSocketBufferPlatformCapability>
MediaDatagramSocketBufferPlatformCapabilityProbe::scan(
    std::uint64_t minimumRequiredEffectiveBytes) noexcept
{
    using Result =
        ::media::Result<MediaDatagramSocketBufferPlatformCapability>;
    if (minimumRequiredEffectiveBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Linux SO_SNDBUF probe target must be positive"));
    }
    MediaLinuxSocketProbeHandle socketHandle(
        ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
    if (socketHandle.get() < 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Linux SO_SNDBUF capability probe socket failed", errno));
    }
    auto minimum = effectiveSendBuffer(socketHandle.get(), 1);
    constexpr int MaximumKernelRequest =
        (std::numeric_limits<int>::max)() / 2;
    auto maximum =
        effectiveSendBuffer(socketHandle.get(), MaximumKernelRequest);
    if (!minimum || !maximum) {
        return Result::failure(
            !minimum ? minimum.error() : maximum.error());
    }
    if (maximum.value() < minimum.value() ||
        maximum.value() % 2U != 0) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Linux SO_SNDBUF capability probe returned inconsistent bounds"));
    }
    return Result::success(MediaDatagramSocketBufferPlatformCapability{
        MediaDatagramSocketBufferAccounting::LinuxDoubled,
        minimum.value(),
        maximum.value(),
        maximum.value() / 2U,
        "Linux socket(7) doubled SO_SNDBUF with kernel min/wmem_max probe"});
}

} // namespace media::ffmpeg::graph

#endif

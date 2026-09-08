#include "internal/graph/planner/realtime/MediaDatagramSocketBufferPlanner.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaDatagramSocketBufferPlan>
MediaDatagramSocketBufferPlanner::plan(
    std::uint64_t minimumRequiredEffectiveBytes,
    const MediaDatagramSocketBufferPlatformCapability& capability) noexcept
{
    using Result = ::media::Result<MediaDatagramSocketBufferPlan>;
    if (minimumRequiredEffectiveBytes == 0 || capability.authority.empty() ||
        capability.minimumEffectiveBytes == 0 ||
        capability.maximumEffectiveBytes < capability.minimumEffectiveBytes ||
        capability.maximumApiRequestedBytes == 0 ||
        capability.maximumApiRequestedBytes > static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram socket buffer requires complete platform capability"));
    }

    const auto requiredEffectiveBytes = (std::max)(
        minimumRequiredEffectiveBytes, capability.minimumEffectiveBytes);
    if (requiredEffectiveBytes > capability.maximumEffectiveBytes) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Datagram socket buffer requirement exceeds platform capability"));
    }
    std::uint64_t apiRequestedBytes = 0;
    std::uint64_t effectiveBytes = 0;
    switch (capability.accounting) {
    case MediaDatagramSocketBufferAccounting::Exact:
        apiRequestedBytes = requiredEffectiveBytes;
        effectiveBytes = requiredEffectiveBytes;
        break;
    case MediaDatagramSocketBufferAccounting::LinuxDoubled:
        apiRequestedBytes = requiredEffectiveBytes / 2U +
            (requiredEffectiveBytes % 2U != 0 ? 1U : 0U);
        if (apiRequestedBytes >
            (std::numeric_limits<std::uint64_t>::max)() / 2U) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Linux Datagram socket buffer accounting overflowed"));
        }
        effectiveBytes = apiRequestedBytes * 2U;
        break;
    default:
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram socket buffer accounting must be explicit"));
    }

    if (apiRequestedBytes > capability.maximumApiRequestedBytes ||
        effectiveBytes > capability.maximumEffectiveBytes) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Datagram SO_SNDBUF product is not representable by the platform"));
    }
    MediaDatagramSocketBufferPlan plan{
        capability.accounting,
        apiRequestedBytes,
        effectiveBytes,
        effectiveBytes,
        effectiveBytes};
    auto status = validateMediaDatagramSocketBufferPlan(plan);
    return status ? Result::success(std::move(plan))
                  : Result::failure(status.error());
}

} // namespace media::ffmpeg::graph

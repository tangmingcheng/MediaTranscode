#include "internal/graph/runtime/network/MediaDatagramSocketBufferApiRequest.h"

#include <limits>

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t>
MediaDatagramSocketBufferApiRequest::fromTargetEffective(
    std::uint64_t targetEffectiveBytes,
    MediaDatagramSocketBufferApiAccounting accounting) noexcept
{
    using Result = ::media::Result<std::uint64_t>;
    if (targetEffectiveBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "socket target effective bytes must be positive"));
    }
    switch (accounting) {
    case MediaDatagramSocketBufferApiAccounting::Exact:
        return Result::success(targetEffectiveBytes);
    case MediaDatagramSocketBufferApiAccounting::LinuxDoubled:
        return Result::success(targetEffectiveBytes / 2U +
                               (targetEffectiveBytes % 2U != 0 ? 1U : 0U));
    }
    return Result::failure(::media::ErrorInfo::invalidArgument(
        "unknown socket buffer API accounting semantics"));
}

} // namespace media::ffmpeg::graph

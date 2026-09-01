#include "internal/graph/planner/realtime/MediaDatagramSocketBufferPlanner.h"

#ifdef _WIN32

#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaDatagramSocketBufferPlatformCapability>
MediaDatagramSocketBufferPlatformCapabilityProbe::scan() noexcept
{
    constexpr auto MaximumSocketInteger =
        static_cast<std::uint64_t>((std::numeric_limits<int>::max)());
    return ::media::Result<
        MediaDatagramSocketBufferPlatformCapability>::success({
        MediaDatagramSocketBufferAccounting::Exact,
        1,
        MaximumSocketInteger,
        MaximumSocketInteger,
        "Microsoft Winsock SO_SNDBUF integer request with exact readback"});
}

} // namespace media::ffmpeg::graph

#endif

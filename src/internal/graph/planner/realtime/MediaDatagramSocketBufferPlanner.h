#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaDatagramSocketBufferPlatformCapability final {
    MediaDatagramSocketBufferAccounting accounting;
    std::uint64_t minimumEffectiveBytes;
    std::uint64_t maximumEffectiveBytes;
    std::uint64_t maximumApiRequestedBytes;
    std::string authority;
};

class MediaDatagramSocketBufferPlatformCapabilityProbe final {
public:
    static ::media::Result<MediaDatagramSocketBufferPlatformCapability>
    scan(
        std::uint64_t minimumRequiredEffectiveBytes,
        MediaIpAddressFamily addressFamily) noexcept;

private:
    MediaDatagramSocketBufferPlatformCapabilityProbe() = delete;
};

class MediaDatagramSocketBufferPlanner final {
public:
    static ::media::Result<MediaDatagramSocketBufferPlan> plan(
        std::uint64_t minimumRequiredEffectiveBytes,
        const MediaDatagramSocketBufferPlatformCapability& capability) noexcept;

private:
    MediaDatagramSocketBufferPlanner() = delete;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/storage/MediaLineageVector.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAacRtpAuHeader final {
    std::size_t size = 0;
    std::uint64_t index = 0;
};

struct MediaAacRtpAuHeaderPlan final {
    explicit MediaAacRtpAuHeaderPlan(MediaLineageAllocationScope allocation)
        : accessUnits(MediaLineageAllocator<MediaAacRtpAuHeader>{std::move(allocation)}) {}
    std::size_t payloadOffset = 0;
    std::size_t payloadBytes = 0;
    std::size_t totalAccessUnitBytes = 0;
    MediaLineageVector<MediaAacRtpAuHeader> accessUnits;
};

class MediaAacRtpAuHeaderPlanner final {
public:
    static ::media::Result<MediaAacRtpAuHeaderPlan> plan(
        std::span<const std::uint8_t> payload, MediaLineageAllocationScope allocation);

private:
    MediaAacRtpAuHeaderPlanner() = delete;
};

} // namespace media::ffmpeg::graph

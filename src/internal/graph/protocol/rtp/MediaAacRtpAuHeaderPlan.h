#pragma once

#include "media_transcode/Result.h"

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
    std::size_t payloadOffset = 0;
    std::size_t payloadBytes = 0;
    std::size_t totalAccessUnitBytes = 0;
    std::vector<MediaAacRtpAuHeader> accessUnits;
};

class MediaAacRtpAuHeaderPlanner final {
public:
    static ::media::Result<MediaAacRtpAuHeaderPlan> plan(
        std::span<const std::uint8_t> payload);

private:
    MediaAacRtpAuHeaderPlanner() = delete;
};

} // namespace media::ffmpeg::graph

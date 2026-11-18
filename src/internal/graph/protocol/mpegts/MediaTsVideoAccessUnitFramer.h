#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsVideoAccessUnitFramer final {
public:
    // Bounds one valid AU; this does not bound the number of AUs in a burst.
    // The caller selects parameter-set injection from the mux policy.
    static ::media::Result<std::size_t> maximumFramedPayloadBytes(
        const MediaTsMaterializedVideoConfig& config,
        std::size_t maximumInputPayloadBytes,
        bool injectParameterSets);

    static ::media::Result<std::span<const std::uint8_t>> frame(
        const MediaTsMuxPlan& plan,
        const MediaTsMaterializedVideoConfig& config,
        std::span<const std::uint8_t> payload,
        bool randomAccess,
        std::vector<std::uint8_t>& workspace);
};

} // namespace media::ffmpeg::graph

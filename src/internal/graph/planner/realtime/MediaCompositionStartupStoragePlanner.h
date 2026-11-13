#pragma once

#include "internal/graph/planner/realtime/MediaStartupRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRtpClockRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaStartupCoordinatorStoragePlanner.h"

namespace media::ffmpeg::graph {

// Components retain their allocation domains. Released audio wrappers use
// ordinary allocations; clock backing follows its existing resource ownership.
// This is not a complete source total or an admission decision.
struct MediaCompositionStartupStoragePlan final {
    MediaStartupBatchStorageCosts batch;
    MediaStartupRouteStoragePlan route;
    MediaRtpClockRouteStoragePlan clock;
    MediaStartupCoordinatorStoragePlan coordinator;
};

class MediaCompositionStartupStoragePlanner final {
public:
    static ::media::Result<MediaCompositionStartupStoragePlan> plan(
        const MediaStartupRoutePlan& route, MediaRtpControlDelivery controlDelivery, std::size_t videoUnits,
        std::optional<std::size_t> audioUnits, std::size_t groupKeyCharacters,
        std::span<const std::size_t> maximumDatagramBytes);
};

} // namespace media::ffmpeg::graph

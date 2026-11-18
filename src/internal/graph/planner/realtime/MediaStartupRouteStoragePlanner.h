#pragma once
#include "internal/graph/planner/realtime/MediaStartupBatchStoragePlanner.h"

namespace media::ffmpeg::graph {
struct MediaStartupRouteStoragePlan final {
    std::uint64_t maximumEnvelopes;
    std::uint64_t maximumReleases;
    std::uint64_t maximumTransactions;
    // Concurrent bounds inside the startup route, not a complete source budget.
    // releasedAudioWrappersBytes includes staging/reservation replacement only;
    // wrappers already published to the ordinary audio branch are additional.
    MediaStartupBatchStorageCosts concurrent;
    // Actual source-resource requests only. Ordinary inner string/wrapper
    // allocations retain their own domain even when an outer owner is Prepared.
    std::size_t preparationBytes;
    // Route-local ordinary requests, not the full downstream Runtime ledger.
    std::size_t ordinaryBytes;
};
class MediaStartupRouteStoragePlanner final {
public:
    static ::media::Result<MediaStartupRouteStoragePlan> plan(
        const MediaStartupRoutePlan& route, std::size_t videoUnits,
        std::optional<std::size_t> audioUnits, const MediaStartupBatchStorageCosts& batch);
};
} // namespace media::ffmpeg::graph

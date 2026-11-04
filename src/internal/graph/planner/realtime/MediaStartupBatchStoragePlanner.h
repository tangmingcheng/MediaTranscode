#pragma once
#include "internal/graph/planner/realtime/MediaStartupRouteMediaPlanner.h"
#include "internal/graph/model/MediaOutputReservationStoragePlan.h"
namespace media::ffmpeg::graph {
struct MediaStartupBatchStorageCosts final {
    std::size_t envelopeWrapperBytes;
    std::size_t releaseWrapperBytes;
    std::size_t transactionWrapperBytes;
    std::size_t releasedArraysBytes;
    std::size_t stagedReferenceArraysBytes;
    std::size_t releaseGroupKeyBytes;
    std::size_t releasedAudioWrappersBytes;
    std::size_t preparationReservationBytes;
    std::size_t extractorReservationBytes;
    std::size_t authorizationScratchBytes;
};
// Composition source frame-audio route, per batch/operation components.
// Wrapper bytes describe one object; audio wrappers and arrays describe one batch.
// Transaction geometry also applies to control transactions. Reanchoring retains both original and new
// release arrays; callers must use actual route lifetimes, not sum these once.
class MediaStartupBatchStoragePlanner final {
public:
    static ::media::Result<MediaStartupBatchStorageCosts> plan(
        const MediaStartupRoutePlan& route, std::size_t videoUnits,
        std::optional<std::size_t> audioUnits, const MediaOutputReservationLayout& reservation,
        std::size_t groupKeyCharacters);
};
}

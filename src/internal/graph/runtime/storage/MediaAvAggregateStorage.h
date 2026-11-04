#pragma once

#include "internal/graph/diagnostics/MediaAggregateIntervalMissObservation.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/MediaAvAggregateCoordination.h"

namespace media::ffmpeg::graph {

struct MediaAvAggregateSourceStorage final {
    MediaFixedCapacityStorage<MediaBufferRef> video;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> purge;
    std::uint64_t minimumGeneration = 0;
    bool videoEnded = false;
    bool displaySealed = false;
    std::uint64_t lastDisplaySequence = 0;
    std::optional<MediaRunningTime> lastDisplayPresentation;
    std::optional<MediaAvAggregateDisplayCommit> lastDisplayCommit;
    bool epochEnded = false;
    bool discardedAudioEnded = false;
    bool prepared = false;
    std::optional<MediaRunningTime> lastInputEnd;
    std::uint64_t committedRealFrames = 0;
    std::uint64_t committedBlackFrames = 0;
    std::uint64_t committedGeneration = 0;
    MediaAggregateGenerationObservation generationObservation;
    std::optional<MediaAggregateIntervalMissObservation> firstIntervalMiss;
};
enum class MediaAvAggregatePendingKind { Video, Audio, VideoEnd, AudioEnd };
struct MediaAvAggregatePendingStorage final {
    MediaAvAggregatePendingKind kind;
    MediaBufferRef buffer;
    MediaFixedCapacityStorage<std::uint64_t> sourceGenerations;
};

} // namespace media::ffmpeg::graph

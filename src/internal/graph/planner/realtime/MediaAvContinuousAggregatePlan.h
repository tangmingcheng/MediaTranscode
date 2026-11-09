#pragma once

#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/model/MediaVideoFrameLineageProduct.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/time/MediaRunningTime.h"

#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvAggregateSourcePlan final {
    MediaAvSyncGroupKey groupKey;
    std::string videoPort;
    std::size_t maximumVideoCandidates;
    std::optional<std::string> discardedAudioPort;
    std::string videoIdentity;
    std::optional<std::string> audioIdentity;
    MediaVideoFrameLineageProduct videoProduct;
};

struct MediaAvAggregateStoragePlan final {
    std::uint64_t fixedStorageBytes;
    std::uint64_t retainedMetadataBytes;
    std::uint64_t maximumRetainedObjects;
};

// Transfers the reservation with its backing; assignment could release the old
// reservation before the old strings/vectors, so it is deliberately unavailable.
struct MediaAvAggregatePreparationOwner final {
    explicit MediaAvAggregatePreparationOwner(MediaPreparationStorageLease&& value) : lease(std::move(value)) {}
    MediaAvAggregatePreparationOwner(MediaAvAggregatePreparationOwner&&) = default;
    MediaAvAggregatePreparationOwner& operator=(MediaAvAggregatePreparationOwner&&) = delete;
    MediaPreparationStorageLease lease;
};
struct MediaAvContinuousAggregateTopology {
    MediaAvAggregatePreparationOwner preparationOwner;
    MediaAvSyncGroupKey outputGroupKey;
    std::vector<MediaAvAggregateSourcePlan> sources;
    std::size_t audioSource;
    std::string audioPort;
    MediaVideoCanvasGeometry canvas;
    MediaRational videoFrameRate;
    MediaResolvedAudioOutputPlan audio;
    MediaRunningTime preparationLead;
    std::uint64_t initialGeneration;
    std::int64_t initialAudioSample;
    std::size_t maximumAudioCandidates;
    std::int64_t maximumAudioCandidateSamples;
    std::size_t maximumAudioContributions;
    std::uint64_t maximumMetadataBytes;
    MediaAvAggregateStoragePlan storage;
};

struct MediaAvContinuousAggregatePlan final : MediaAvContinuousAggregateTopology {
    MediaVideoCanvasStorage canvasStorage;
};

} // namespace media::ffmpeg::graph

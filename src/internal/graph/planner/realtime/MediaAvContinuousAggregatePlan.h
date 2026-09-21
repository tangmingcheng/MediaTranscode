#pragma once

#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvAggregateSourcePlan final {
    MediaAvSyncGroupKey groupKey;
    std::string videoPort;
    std::size_t maximumVideoCandidates;
    std::optional<std::string> discardedAudioPort;
};

struct MediaAvContinuousAggregatePlan final {
    MediaAvSyncGroupKey outputGroupKey;
    std::vector<MediaAvAggregateSourcePlan> sources;
    std::size_t audioSource;
    std::string audioPort;
    MediaVideoCanvasPlan canvas;
    MediaRational videoFrameRate;
    MediaResolvedAudioOutputPlan audio;
    MediaRunningTime preparationLead;
    std::uint64_t initialGeneration;
    std::int64_t initialAudioSample;
    std::size_t maximumAudioCandidates;
    std::int64_t maximumAudioCandidateSamples;
    std::size_t maximumAudioContributions;
    std::uint64_t maximumMetadataBytes;
};

} // namespace media::ffmpeg::graph

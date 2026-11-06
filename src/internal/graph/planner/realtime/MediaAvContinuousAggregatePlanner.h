#pragma once

#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include <string_view>
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlan.h"
#include <span>

namespace media::ffmpeg::graph {
struct MediaAudioCorrectionReachabilityPlan;
// Synchronous borrowed source facts; the resulting aggregate owns its strings.
// No source allocation, sealed admission or graph identity is needed here.
struct MediaAvAggregateSourceFacts final {
    const MediaAvSyncGroupKey& groupKey;
    std::optional<MediaTranscodeStreamSet> members;
    std::optional<MediaAvSyncDomainRole> domainRole;
    std::optional<MediaVideoStartupReadiness> videoReadiness;
    bool sourceDisplayPoints;
    MediaRational frameRate;
    std::size_t frameQueueCapacity;
    MediaRunningTime activationOutputLead;
    std::uint64_t initialGeneration;
    std::string_view videoIdentity;
    std::optional<std::string_view> audioIdentity;
    bool audioEnabled;
    const MediaResolvedAudioOutputPlan* audioOutput;
    const MediaAudioCorrectionReachabilityPlan* audioCorrection;
};
class MediaAvContinuousAggregatePlanner final {
public:
    // Candidate slots retained by the aggregate, not unique source allocations.
    static ::media::Result<std::size_t> videoCandidateCapacity(
        std::size_t frameQueueCapacity, bool sourceDisplayPoints,
        MediaRational sourceFrameRate, MediaRunningTime outputLead);
    static ::media::Result<MediaAvContinuousAggregateTopology> plan(
        std::span<const MediaAvAggregateSourceFacts> sources,
        const MediaRealtimeAvOutputRuntimePlan& output,
        MediaVideoCanvasGeometry canvas, MediaRational outputFrameRate,
        std::size_t audioSource);
    // Same immutable prepared declarations as plan(); only queue/reachability
    // capacities may change. Output/canvas/identity backing remains owned.
    // Success allocates nothing. A failed refresh invalidates the workspace.
    static ::media::Status rebindCapacity(
        std::span<const MediaAvAggregateSourceFacts> sources,
        MediaAvContinuousAggregateTopology& plan);
private:
    MediaAvContinuousAggregatePlanner() = delete;
};
} // namespace media::ffmpeg::graph

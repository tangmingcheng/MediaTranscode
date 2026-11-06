#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"

namespace media::ffmpeg::graph {
// Fixed identities/timing are borrowed from a stable owning assembly. Only the
// acquiring quantities are candidate values; constructing these views allocates nothing.
struct MediaVideoInputAssemblyView final {
    const std::string& sourceIdentity;
    const MediaVideoInputAssemblyTiming& timing;
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
    MediaVideoInputAssemblyView(const MediaVideoInputAssemblyPlan& plan)
        : sourceIdentity(plan.sourceIdentity), timing(plan.timing), acquiringCapacity(plan.acquiringCapacity),
          acquiringTimeout(plan.acquiringTimeout) {}
    MediaVideoInputAssemblyView(MediaVideoInputAssemblyPlan&&) = delete;
    MediaVideoInputAssemblyView(const MediaVideoInputAssemblyPlan&&) = delete;
};
struct MediaCanonicalAudioAssemblyView final {
    const std::string& sourceIdentity;
    const MediaCanonicalAudioDurationPlan& duration;
    MediaDecodeOrderMode decodeOrder;
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
    MediaCanonicalAudioAssemblyView(const MediaCanonicalAudioAssemblyPlan& plan)
        : sourceIdentity(plan.sourceIdentity), duration(plan.duration), decodeOrder(plan.decodeOrder),
          acquiringCapacity(plan.acquiringCapacity), acquiringTimeout(plan.acquiringTimeout) {}
    MediaCanonicalAudioAssemblyView(MediaCanonicalAudioAssemblyPlan&&) = delete;
    MediaCanonicalAudioAssemblyView(const MediaCanonicalAudioAssemblyPlan&&) = delete;
};
// Keep the owning plan alive and unmoved. Numeric updates belong to this view,
// not the owner. Consumers validate the updated view before publishing quantities.
struct MediaRealtimeAvSyncAssemblyView final {
    const MediaAvSyncInputClockPlan& inputClock;
    MediaInitialGenerationPolicy generationPolicy;
    std::uint64_t initialGeneration;
    MediaClockEvidencePolicy evidencePolicy;
    MediaVideoInputAssemblyView video;
    std::optional<MediaCanonicalAudioAssemblyView> audio;
    MediaRealtimeAvSyncAssemblyView(const MediaRealtimeAvSyncAssemblyPlan& plan)
        : inputClock(plan.inputClock), generationPolicy(plan.generationPolicy), initialGeneration(plan.initialGeneration),
          evidencePolicy(plan.evidencePolicy), video(plan.video) {
        if (plan.audio) audio.emplace(*plan.audio);
    }
    MediaRealtimeAvSyncAssemblyView(MediaRealtimeAvSyncAssemblyPlan&&) = delete;
    MediaRealtimeAvSyncAssemblyView(const MediaRealtimeAvSyncAssemblyPlan&&) = delete;
};
} // namespace media::ffmpeg::graph

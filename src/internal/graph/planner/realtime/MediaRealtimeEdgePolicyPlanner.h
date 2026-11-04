#pragma once

#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaAvSyncStartupPolicy;
struct MediaRealtimeGraphResourceLedgerPlan;

class MediaRealtimeEdgePolicyPlanner final {
public:
    static ::media::Result<MediaEdgePolicy> planCanonicalAudioInput(
        MediaBranchMode mode, const MediaEdgePolicy& outputPacket,
        const std::optional<MediaEdgePolicy>& sourceRelease);
    static ::media::Result<MediaRealtimeEdgePolicySet> planSynchronizedOutput(
        const MediaRealtimeGraphResourceLedgerPlan& resources);
    static ::media::Result<MediaRealtimeEdgePolicySet> planSynchronizedSource(
        const MediaGraphQueueParameters& queues,
        const MediaAvSyncStartupPolicy& startup,
        MediaTranscodeStreamSet members);
    static MediaRealtimeEdgePolicySet plan(
        const MediaGraphQueueParameters& queues);
    static ::media::Result<MediaRealtimeEdgePolicySet>
    planWithSynchronizedPacketMemoryBudget(
        const MediaGraphQueueParameters& queues,
        std::uint64_t maximumBytes,
        std::size_t maximumBuffers);
    static ::media::Result<MediaRealtimeEdgePolicySet>
    planWithAvStartupRelease(
        const MediaGraphQueueParameters& queues,
        std::uint64_t maximumBytes,
        std::size_t maximumBuffers,
        std::size_t maximumVideoReleaseUnits,
        std::size_t maximumAudioReleaseUnits);

private:
    MediaRealtimeEdgePolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph

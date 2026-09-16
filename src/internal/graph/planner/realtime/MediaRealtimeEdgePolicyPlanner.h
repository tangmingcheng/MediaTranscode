#pragma once

#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRealtimeEdgePolicyPlanner final {
public:
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

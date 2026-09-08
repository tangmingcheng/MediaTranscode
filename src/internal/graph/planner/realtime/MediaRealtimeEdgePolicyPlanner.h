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

private:
    MediaRealtimeEdgePolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph

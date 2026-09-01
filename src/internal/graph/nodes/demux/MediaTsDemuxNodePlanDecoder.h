#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/protocol/mpegts/MediaTsInitialAcquiringPacketBuffer.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"
#include "internal/graph/protocol/mpegts/MediaTsRuntimeBinding.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaTsDemuxNodeRuntimePlan final {
    std::size_t packetStride;
    std::size_t evidenceCapacity;
    std::size_t projectionCapacity;
    std::uint64_t maximumPositionRegressionBytes;
    std::size_t pesProvenanceCapacity;
    std::uint64_t initialSourceGeneration;
    std::uint64_t initialRawTransportGeneration;
    MediaTsProgramClockPolicy clockPolicy;
    MediaTsInitialPacketRetentionPlan retention;
};

class MediaTsDemuxNodePlanDecoder final {
public:
    static ::media::Result<MediaTsDemuxNodeRuntimePlan> decode(
        const MediaNodeOptions* options,
        const MediaTsRuntimeBinding& binding);

private:
    MediaTsDemuxNodePlanDecoder() = delete;
};

} // namespace media::ffmpeg::graph

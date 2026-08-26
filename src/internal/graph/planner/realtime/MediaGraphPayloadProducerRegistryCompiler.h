#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaGraphPayloadProducerRegistryCompiler final {
public:
    static ::media::Result<MediaGraphPayloadCreditPlan> compile(
        const MediaGraph& graph,
        const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
        std::uint64_t availablePayloadBytes,
        std::uint64_t maximumPayloadObjects,
        bool runtimeIntegrationComplete);

private:
    MediaGraphPayloadProducerRegistryCompiler() = delete;
};

} // namespace media::ffmpeg::graph

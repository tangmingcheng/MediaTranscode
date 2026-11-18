#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/model/MediaGraphPayloadProducerFact.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaGraphPayloadProducerRegistryCompiler final {
public:
    static ::media::Result<MediaGraphPayloadCreditPlan> compile(
        const MediaGraph& graph,
        std::span<const MediaGraphPayloadProducerFact> facts,
        std::uint64_t availablePayloadBytes,
        std::uint64_t maximumPayloadObjects,
        std::span<const MediaNodeId> selectedNodes);

private:
    MediaGraphPayloadProducerRegistryCompiler() = delete;
};

} // namespace media::ffmpeg::graph

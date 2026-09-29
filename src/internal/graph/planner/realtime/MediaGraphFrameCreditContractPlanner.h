#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaGraphFrameCreditContractPlanner final {
public:
    static ::media::Result<MediaFrameCreditContract> plan(
        const MediaNode& node, std::uint64_t maximumLogicalBytes);
};
} // namespace media::ffmpeg::graph

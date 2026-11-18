#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/planner/video/MediaPipelineStagePlan.h"
#include "internal/graph/model/MediaVideoDecodeLineageProduct.h"

namespace media::ffmpeg::graph {

struct MediaVideoDecoderLineageOptions final {
    MediaVideoLineagePropagation propagation;
    std::size_t capacity;
    MediaVideoDecodeLineageProduct product;
};

// Serializes a selected decoder without requiring an encoder or constructing a DAG.
class MediaVideoDecoderPlanOptionCodec final {
public:
    // nullopt explicitly leaves lineage unconfigured.
    static ::media::Result<MediaNodeOptions> encode(
        const MediaPipelineStagePlan& decoder,
        std::optional<MediaVideoDecoderLineageOptions> lineage);
private:
    MediaVideoDecoderPlanOptionCodec() = delete;
};

} // namespace media::ffmpeg::graph

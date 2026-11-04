#pragma once
#include "internal/graph/planner/capability/MediaDecoderOutputAllocationAdapter.h"
#include "internal/graph/model/MediaVideoFrameReadinessPlan.h"
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"
struct AVFrame;
struct AVFilterGraph;

namespace media::ffmpeg::graph {
struct MediaPreparedFilterProducerFacts final {
    MediaVideoFrameProducerFacts producer;
    bool aliasesInput;
    std::optional<MediaVideoFilterIsolationEvidence> isolation;
};
class MediaVideoSourceProducerAdapter final {
public:
    static ::media::Result<MediaVideoFrameProducerFacts> decoder(
        const MediaDecoderOutputAllocation& allocation, const AVFrame& frame);
    static ::media::Result<MediaPreparedFilterProducerFacts> filter(
        const AVFilterGraph& graph, std::string description, const AVFrame& input,
        const MediaVideoFrameProducerFacts& inputProducer);
    static ::media::Status validateOutput(const AVFrame& frame,
        const MediaVideoFrameProducerFacts& producer);
};
}

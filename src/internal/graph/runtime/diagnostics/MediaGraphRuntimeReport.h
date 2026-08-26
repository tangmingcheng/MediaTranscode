#pragma once

#include "internal/graph/runtime/backpressure/MediaBackpressureController.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaDroppedEdgeReport final {
    MediaEdgeId edgeId;
    std::uint64_t droppedBuffers = 0;
};

struct MediaGraphRuntimeReport {
    MediaGraphRuntimeState state = MediaGraphRuntimeState::Empty;
    MediaGraphRuntimeMetrics metrics;
    MediaBackpressureReport backpressure;
    std::vector<MediaDroppedEdgeReport> droppedEdges;
    std::optional<MediaGraphPayloadCreditSnapshot> payloadCredits;

    std::string summary() const;
};

class MediaGraphRuntimeReporter final {
public:
    static MediaGraphRuntimeReport capture(MediaGraphRuntime& runtime);
    static MediaGraphRuntimeReport capture(const MediaGraphRuntime& runtime);
};

} // namespace media::ffmpeg::graph

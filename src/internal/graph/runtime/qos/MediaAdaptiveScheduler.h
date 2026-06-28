#pragma once

#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/qos/MediaQosController.h"
#include "internal/graph/runtime/streaming/MediaStreamingMetrics.h"

namespace media::ffmpeg::graph {

enum class MediaAdaptiveSchedulerDecisionKind {
    Keep,
    DropFrame,
    RequestKeyFrame,
    ReduceBitrate,
    IncreaseBitrate
};

struct MediaAdaptiveSchedulerDecision {
    MediaAdaptiveSchedulerDecisionKind kind = MediaAdaptiveSchedulerDecisionKind::Keep;
    MediaQosAction qosAction;
};

class MediaAdaptiveScheduler final {
public:
    MediaAdaptiveSchedulerDecision update(const MediaGraphRuntimeReport& report,
                                          const MediaStreamingMetrics& metrics);

private:
    MediaQosController m_qos;
};

} // namespace media::ffmpeg::graph

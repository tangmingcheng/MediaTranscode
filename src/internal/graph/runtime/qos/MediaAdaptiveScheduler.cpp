#include "internal/graph/runtime/qos/MediaAdaptiveScheduler.h"

namespace media::ffmpeg::graph {

MediaAdaptiveSchedulerDecision MediaAdaptiveScheduler::update(const MediaGraphRuntimeReport& report,
                                                               const MediaStreamingMetrics& metrics)
{
    MediaAdaptiveBitrateSample sample;
    sample.throughputBitsPerSecond = metrics.estimatedThroughputBitsPerSecond;
    sample.queueDelayUs = metrics.queueDelayUs;
    sample.packetLossRate = metrics.packetLossRate;

    MediaAdaptiveSchedulerDecision decision;
    decision.qosAction = m_qos.update(report, sample);

    if (!report.backpressure.healthy()) {
        decision.kind = MediaAdaptiveSchedulerDecisionKind::DropFrame;
        return decision;
    }

    switch (decision.qosAction.kind) {
    case MediaQosActionKind::ReduceBitrate:
        decision.kind = MediaAdaptiveSchedulerDecisionKind::ReduceBitrate;
        break;
    case MediaQosActionKind::IncreaseBitrate:
        decision.kind = MediaAdaptiveSchedulerDecisionKind::IncreaseBitrate;
        break;
    case MediaQosActionKind::RequestKeyFrame:
        decision.kind = MediaAdaptiveSchedulerDecisionKind::RequestKeyFrame;
        break;
    default:
        decision.kind = MediaAdaptiveSchedulerDecisionKind::Keep;
        break;
    }

    return decision;
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeAvSyncPlanningFacts>
MediaRealtimeAvSyncPlanningFactsResolver::resolve(
    const MediaRealtimeRtpTranscodePlan& plan,
    const MediaAvSyncPlan& synchronization)
{
    if (!plan.audioPlan.resolvedOutput || !plan.avSyncComponentBounds ||
        !synchronization.audioServo.outputSampleRate) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "synchronized planning requires codec, resampler, and servo timing facts"));
    }
    const auto& output = *plan.audioPlan.resolvedOutput;
    const auto& bounds = *plan.avSyncComponentBounds;
    if (bounds.decoderDelaySamples < 0 || bounds.decodeQueueSamples <= 0 ||
        bounds.resampleQueueSamples <= 0 || bounds.encodeQueueSamples <= 0 ||
        bounds.schedulerQueueSamples <= 0 || bounds.protocolBatchSamples <= 0 ||
        bounds.mailboxDeliveryMarginSamples <= 0 ||
        bounds.maximumResamplerOutputBlockSamples <= 0 ||
        bounds.mailboxCapacity == 0) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized component bounds must be explicit and positive"));
    }

    MediaRealtimeAvSyncPlanningFacts facts;
    facts.outputSampleRate = output.sampleRate();
    facts.decoderDelaySamples = bounds.decoderDelaySamples;
    facts.encoderLookaheadSamples = output.encoderDelaySamples();
    facts.decodeQueueSamples = bounds.decodeQueueSamples;
    facts.resampleQueueSamples = bounds.resampleQueueSamples;
    facts.encodeQueueSamples = bounds.encodeQueueSamples;
    facts.schedulerQueueSamples = bounds.schedulerQueueSamples;
    facts.protocolBatchSamples = bounds.protocolBatchSamples;
    facts.mailboxDeliveryMarginSamples = bounds.mailboxDeliveryMarginSamples;
    facts.maximumResamplerOutputBlockSamples = bounds.maximumResamplerOutputBlockSamples;
    facts.mailboxCapacity = bounds.mailboxCapacity;
    facts.acknowledgementTimeout = synchronization.recovery.reacquisitionTimeoutNs;
    facts.terminalDrainWindow = synchronization.audioServo.maximumMeasurementGapNs;
    return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::success(std::move(facts));
}

} // namespace media::ffmpeg::graph

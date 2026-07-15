#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::int64_t> checkedSamples(std::size_t capacity,
                                             std::int64_t unitSamples,
                                             const char* owner)
{
    if (capacity == 0 || unitSamples <= 0 ||
        capacity > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max() / unitSamples)) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(owner) + " does not have a representable sample bound"));
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(capacity) * unitSamples);
}

} // namespace

::media::Result<MediaRealtimeAvSyncPlanningFacts>
MediaRealtimeAvSyncPlanningFactsResolver::resolve(
    const MediaRealtimeRtpTranscodePlan& plan,
    const MediaAvSyncPlan& synchronization)
{
    if (!plan.audioPlan.resolvedOutput ||
        !plan.audioPlan.decoderDelaySamples ||
        !plan.audioPlan.maximumResamplerOutputBlockSamples ||
        !synchronization.audioServo.outputSampleRate ||
        !synchronization.audioServo.commandLeadNs) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "synchronized planning requires codec, resampler, and servo timing facts"));
    }
    const auto& output = *plan.audioPlan.resolvedOutput;
    const auto unitSamples = static_cast<std::int64_t>(output.codecFrameSamples());
    auto decodeQueue = checkedSamples(plan.queues.packet, unitSamples, "audio decode queue");
    auto resampleQueue = checkedSamples(plan.queues.frame, unitSamples, "audio resample queue");
    auto encodeQueue = checkedSamples(plan.queues.frame, unitSamples, "audio encode queue");
    auto schedulerQueue = checkedSamples(plan.queues.mux, unitSamples, "audio scheduler queue");
    auto mailboxMargin = checkedSamples(plan.queues.metadata, unitSamples, "audio correction mailbox delivery");
    if (!decodeQueue || !resampleQueue || !encodeQueue || !schedulerQueue ||
        !mailboxMargin) {
        const auto* error = !decodeQueue ? &decodeQueue.error()
            : !resampleQueue ? &resampleQueue.error()
            : !encodeQueue ? &encodeQueue.error()
            : !schedulerQueue ? &schedulerQueue.error()
            : &mailboxMargin.error();
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(*error);
    }

    MediaRealtimeAvSyncPlanningFacts facts;
    facts.outputSampleRate = output.sampleRate();
    facts.decoderDelaySamples = *plan.audioPlan.decoderDelaySamples;
    facts.encoderLookaheadSamples = output.encoderDelaySamples();
    facts.decodeQueueSamples = decodeQueue.value();
    facts.resampleQueueSamples = resampleQueue.value();
    facts.encodeQueueSamples = encodeQueue.value();
    facts.schedulerQueueSamples = schedulerQueue.value();
    facts.protocolBatchSamples = unitSamples;
    facts.mailboxDeliveryMarginSamples = mailboxMargin.value();
    facts.maximumResamplerOutputBlockSamples =
        *plan.audioPlan.maximumResamplerOutputBlockSamples;
    facts.mailboxCapacity = plan.queues.metadata;
    facts.acknowledgementTimeout = synchronization.recovery.reacquisitionTimeoutNs;
    facts.terminalDrainWindow = synchronization.audioServo.maximumMeasurementGapNs;
    return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::success(std::move(facts));
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

namespace media::ffmpeg::graph {
namespace {

bool validPacketDurationEvidence(
    const MediaTsPacketDurationEvidence& evidence,
    int streamIndex,
    int elementaryPid) noexcept
{
    return evidence.streamIndex == streamIndex &&
        evidence.elementaryPid == elementaryPid &&
        evidence.packetDuration > 0 && evidence.timeBase.num > 0 &&
        evidence.timeBase.den > 0;
}

} // namespace

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
        bounds.schedulerQueueSamples <= 0 ||
        bounds.mailboxDeliveryMarginSamples <= 0 ||
        bounds.maximumResamplerOutputBlockSamples <= 0 ||
        bounds.mailboxCapacity == 0) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized component bounds must be explicit and positive"));
    }

    MediaRealtimeAvSyncPlanningFacts facts;
    facts.inputVideoIdentity = synchronization.startup.videoIdentity;
    facts.inputAudioIdentity = synchronization.startup.audioIdentity;
    facts.outputSampleRate = output.sampleRate();
    facts.decoderDelaySamples = bounds.decoderDelaySamples;
    facts.encoderLookaheadSamples = output.encoderDelaySamples();
    facts.decodeQueueSamples = bounds.decodeQueueSamples;
    facts.resampleQueueSamples = bounds.resampleQueueSamples;
    facts.encodeQueueSamples = bounds.encodeQueueSamples;
    facts.schedulerQueueSamples = bounds.schedulerQueueSamples;
    if (synchronization.topology ==
        MediaAvSyncTopology::SeparateRtpToSeparateRtp) {
        if (!synchronization.rtp ||
            !synchronization.rtp->videoInput.clockRate ||
            !synchronization.rtp->audioInput.clockRate ||
            !plan.audioInput.rtpDepacketizer ||
            plan.audioInput.rtpDepacketizer->accessUnitDurationRtpTicks <= 0 ||
            !plan.videoOutput.scheduledPacketization ||
            !plan.audioOutput.scheduledPacketization ||
            !plan.audioOutput.scheduledPacketization->maximumAccessUnitSamples()) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "scheduled RTP packetization does not publish audio batch timing"));
        }
        facts.inputVideoClockRate = *synchronization.rtp->videoInput.clockRate;
        facts.inputAudioSampleRate = *synchronization.rtp->audioInput.clockRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            plan.audioInput.rtpDepacketizer->accessUnitDurationRtpTicks);
        facts.protocolBatchSamples =
            *plan.audioOutput.scheduledPacketization->maximumAccessUnitSamples();
    } else if (synchronization.topology == MediaAvSyncTopology::MpegTsToMpegTs) {
        if (!plan.audioPlan.selectedDecoder ||
            plan.audioPlan.selectedDecoder->inputSampleRate <= 0 ||
            !synchronization.ts || !synchronization.ts->outputMux ||
            !synchronization.ts->videoPid || !synchronization.ts->audioPid ||
            !plan.input.mpegTs ||
            !plan.input.mpegTs->videoPacketDuration ||
            !plan.input.mpegTs->audioPacketDuration ||
            !validPacketDurationEvidence(
                *plan.input.mpegTs->videoPacketDuration,
                plan.videoPlan.sourceStreamIndex,
                *synchronization.ts->videoPid) ||
            !validPacketDurationEvidence(
                *plan.input.mpegTs->audioPacketDuration,
                plan.audioPlan.sourceStreamIndex,
                *synchronization.ts->audioPid) ||
            synchronization.ts->outputMux->parameters()
                    .maximumAudioAccessUnitSamples <= 0) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS mux selection does not publish audio batch timing"));
        }
        facts.inputAudioSampleRate =
            plan.audioPlan.selectedDecoder->inputSampleRate;
        facts.inputVideoPacketDuration =
            plan.input.mpegTs->videoPacketDuration;
        facts.inputAudioPacketDuration =
            plan.input.mpegTs->audioPacketDuration;
        facts.protocolBatchSamples = synchronization.ts->outputMux->parameters()
                                         .maximumAudioAccessUnitSamples;
    } else {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::unsupported(
                "synchronized protocol batch topology is unsupported"));
    }
    facts.mailboxDeliveryMarginSamples = bounds.mailboxDeliveryMarginSamples;
    facts.maximumResamplerOutputBlockSamples = bounds.maximumResamplerOutputBlockSamples;
    facts.mailboxCapacity = bounds.mailboxCapacity;
    facts.acknowledgementTimeout = synchronization.recovery.reacquisitionTimeoutNs;
    facts.terminalDrainWindow = synchronization.audioServo.maximumMeasurementGapNs;
    return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::success(std::move(facts));
}

} // namespace media::ffmpeg::graph

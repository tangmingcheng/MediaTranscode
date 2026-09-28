#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeAvOutputTimingFacts>
MediaRealtimeAvSyncPlanningFactsResolver::resolveOutput(
    const MediaResolvedAudioOutputPlan& audio,
    const MediaRealtimeOutputPlanningDraft& plannedOutput,
    const MediaAvSyncPlan& synchronization)
{
    MediaRealtimeAvOutputTimingFacts facts;
    facts.outputSampleRate = audio.sampleRate();
    if (synchronization.rtpOutput.has_value() ==
        synchronization.projectMpegTsOutput.has_value()) {
        return ::media::Result<MediaRealtimeAvOutputTimingFacts>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized output requires exactly one protocol authority"));
    }
    if (synchronization.rtpOutput) {
        if (!plannedOutput.videoOutput.scheduledPacketization ||
            !plannedOutput.audioOutput.scheduledPacketization ||
            !plannedOutput.audioOutput.scheduledPacketization
                 ->maximumAccessUnitSamples()) {
            return ::media::Result<MediaRealtimeAvOutputTimingFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "scheduled RTP output does not publish audio batch timing"));
        }
        facts.outputVideoRtpPacketization =
            plannedOutput.videoOutput.scheduledPacketization;
        facts.outputAudioRtpPacketization =
            plannedOutput.audioOutput.scheduledPacketization;
        facts.protocolBatchSamples =
            *plannedOutput.audioOutput.scheduledPacketization
                 ->maximumAccessUnitSamples();
    } else {
        const auto* program = synchronization.projectMpegTsOutput->outputMux
            ? synchronization.projectMpegTsOutput->outputMux
                  ->audioVideoProgram()
            : nullptr;
        if (!program || program->maximumAudioAccessUnitSamples <= 0) {
            return ::media::Result<MediaRealtimeAvOutputTimingFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output does not publish audio batch timing"));
        }
        facts.protocolBatchSamples =
            program->maximumAudioAccessUnitSamples;
    }
    return ::media::Result<MediaRealtimeAvOutputTimingFacts>::success(std::move(facts));
}

::media::Result<MediaRealtimeAvSyncResolvedFacts>
MediaRealtimeAvSyncPlanningFactsResolver::resolve(
    const MediaRealtimeRtpTranscodePlanCore& plan,
    const MediaAudioPipelinePlan& audio,
    const MediaRealtimeAvSyncComponentBounds& componentBounds,
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput,
    const MediaRealtimeOutputPlanningDraft& plannedOutput,
    const MediaAvSyncPlan& synchronization)
{
    if (!audio.resolvedOutput ||
        !synchronization.audioServo.outputSampleRate) {
        return ::media::Result<MediaRealtimeAvSyncResolvedFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "synchronized planning requires codec, resampler, and servo timing facts"));
    }
    const auto& output = *audio.resolvedOutput;
    const auto* copyBounds =
        std::get_if<MediaSynchronizedAudioPacketCopyBounds>(&componentBounds);
    const auto* transcodeBounds =
        std::get_if<MediaSynchronizedAudioFrameTranscodeBounds>(&componentBounds);
    const bool validCopy = copyBounds &&
        audio.branchMode == MediaBranchMode::CopyPacket &&
        audio.maximumAccessUnitSamples &&
        *audio.maximumAccessUnitSamples == copyBounds->accessUnitSamples &&
        copyBounds->accessUnitSamples > 0 &&
        copyBounds->schedulerQueueSamples > 0 &&
        !audio.selectedDecoder && !audio.selectedResampler;
    const bool validTranscode = transcodeBounds &&
        audio.branchMode == MediaBranchMode::TranscodeFrame &&
        transcodeBounds->decoderDelaySamples >= 0 &&
        transcodeBounds->decodeQueueSamples > 0 &&
        transcodeBounds->resampleQueueSamples > 0 &&
        transcodeBounds->encodeQueueSamples > 0 &&
        transcodeBounds->schedulerQueueSamples > 0 &&
        transcodeBounds->mailboxDeliveryMarginSamples > 0 &&
        transcodeBounds->maximumResamplerOutputBlockSamples > 0 &&
        transcodeBounds->mailboxCapacity > 0;
    if (!validCopy && !validTranscode) {
        return ::media::Result<MediaRealtimeAvSyncResolvedFacts>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized component bounds conflict with the audio branch"));
    }

    MediaRealtimeAvSyncPlanningFacts facts;
    if (copyBounds) {
        facts.schedulerQueueSamples = copyBounds->schedulerQueueSamples;
    } else {
        facts.decoderDelaySamples = transcodeBounds->decoderDelaySamples;
        facts.encoderLookaheadSamples = output.encoderDelaySamples();
        facts.decodeQueueSamples = transcodeBounds->decodeQueueSamples;
        facts.resampleQueueSamples = transcodeBounds->resampleQueueSamples;
        facts.encodeQueueSamples = transcodeBounds->encodeQueueSamples;
        facts.schedulerQueueSamples = transcodeBounds->schedulerQueueSamples;
    }
    auto source = MediaRealtimeAvSourceClockPlanner::plan(
        {plan.inputType, plan.inputLayout, plan.input, plan.videoPlan.sourceStreamIndex,
         plan.videoPlan.inputCodecName, audio, isolatedAudioInput}, synchronization);
    if (!source) return ::media::Result<MediaRealtimeAvSyncResolvedFacts>::failure(source.error());
    static_cast<MediaRealtimeAvSourceTimingFacts&>(facts) = source.value().timing;

    auto outputFacts = resolveOutput(output, plannedOutput, synchronization);
    if (!outputFacts) return ::media::Result<MediaRealtimeAvSyncResolvedFacts>::failure(outputFacts.error());
    static_cast<MediaRealtimeAvOutputTimingFacts&>(facts) = std::move(outputFacts).value();
    if (copyBounds) {
        if (!facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate != output.sampleRate() ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit !=
                copyBounds->accessUnitSamples ||
            !facts.protocolBatchSamples ||
            *facts.protocolBatchSamples != copyBounds->accessUnitSamples) {
            return ::media::Result<MediaRealtimeAvSyncResolvedFacts>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "synchronized packet-copy timing domains conflict"));
        }
    } else {
        facts.mailboxDeliveryMarginSamples =
            transcodeBounds->mailboxDeliveryMarginSamples;
        facts.maximumResamplerOutputBlockSamples =
            transcodeBounds->maximumResamplerOutputBlockSamples;
        facts.mailboxCapacity = transcodeBounds->mailboxCapacity;
    }
    facts.acknowledgementTimeout = synchronization.recovery.reacquisitionTimeoutNs;
    facts.terminalDrainWindow = synchronization.audioServo.maximumMeasurementGapNs;
    return ::media::Result<MediaRealtimeAvSyncResolvedFacts>::success({std::move(facts), std::move(source).value().assembly});
}

} // namespace media::ffmpeg::graph

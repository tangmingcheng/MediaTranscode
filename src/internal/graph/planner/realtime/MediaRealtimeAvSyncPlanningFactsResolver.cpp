#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <limits>

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
    const MediaRealtimeRtpTranscodePlanCore& plan,
    const MediaAudioPipelinePlan& audio,
    const MediaRealtimeAvSyncComponentBounds& componentBounds,
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput,
    const MediaRealtimeOutputPlanningDraft& plannedOutput,
    const MediaAvSyncPlan& synchronization)
{
    if (!audio.resolvedOutput ||
        !synchronization.audioServo.outputSampleRate) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
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
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized component bounds conflict with the audio branch"));
    }

    MediaRealtimeAvSyncPlanningFacts facts;
    facts.inputVideoIdentity = synchronization.startup.videoIdentity;
    facts.inputAudioIdentity = synchronization.startup.audioIdentity;
    facts.outputSampleRate = output.sampleRate();
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
    if (!synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "synchronized input clock mode is missing"));
    }
    if (*synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (!synchronization.rtpInput ||
            !synchronization.rtpInput->videoInput.clockRate ||
            !synchronization.rtpInput->audioInput.clockRate ||
            !isolatedAudioInput || !isolatedAudioInput->rtpDepacketizer ||
            isolatedAudioInput->rtpDepacketizer->accessUnitDurationRtpTicks <= 0) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "RTP input clock does not publish complete duration facts"));
        }
        facts.inputVideoClockRate =
            *synchronization.rtpInput->videoInput.clockRate;
        facts.inputAudioSampleRate =
            *synchronization.rtpInput->audioInput.clockRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            isolatedAudioInput->rtpDepacketizer->accessUnitDurationRtpTicks);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        const auto* selectedProgram = plan.input.mpegTs
            ? std::get_if<MediaTsAudioVideoSelectedProgramPlan>(
                  &plan.input.mpegTs->selectedProgram)
            : nullptr;
        const int inputSampleRate = copyBounds
            ? output.sampleRate()
            : audio.selectedDecoder
                ? audio.selectedDecoder->inputSampleRate
                : 0;
        const std::int64_t inputAccessUnitSamples = copyBounds
            ? copyBounds->accessUnitSamples
            : audio.selectedDecoder
                ? audio.selectedDecoder->maximumOutputBlockInputSamples
                : 0;
        if (inputSampleRate <= 0 || inputAccessUnitSamples <= 0 ||
            inputAccessUnitSamples >
                std::numeric_limits<std::uint32_t>::max() ||
            !synchronization.mpegTsInput ||
            !synchronization.mpegTsInput->videoPid ||
            !synchronization.mpegTsInput->audioPid ||
            !selectedProgram ||
            !validPacketDurationEvidence(
                selectedProgram->videoPacketDuration,
                plan.videoPlan.sourceStreamIndex,
                *synchronization.mpegTsInput->videoPid) ||
            !validPacketDurationEvidence(
                selectedProgram->audioPacketDuration,
                audio.sourceStreamIndex,
                *synchronization.mpegTsInput->audioPid)) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS input clock does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate = inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            inputAccessUnitSamples);
        facts.inputVideoPacketDuration =
            selectedProgram->videoPacketDuration;
        facts.inputAudioPacketDuration =
            selectedProgram->audioPacketDuration;
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        const int inputSampleRate = copyBounds
            ? output.sampleRate()
            : audio.selectedDecoder
                ? audio.selectedDecoder->inputSampleRate
                : 0;
        const std::int64_t inputAccessUnitSamples = copyBounds
            ? copyBounds->accessUnitSamples
            : audio.selectedDecoder
                ? audio.selectedDecoder->maximumOutputBlockInputSamples
                : 0;
        if (!synchronization.demuxTimestampInput ||
            inputSampleRate <= 0 || inputAccessUnitSamples <= 0 ||
            inputAccessUnitSamples >
                std::numeric_limits<std::uint32_t>::max()) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp input does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate = inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            inputAccessUnitSamples);
    } else {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::unsupported(
                "synchronized input clock mode is unsupported"));
    }

    if (synchronization.rtpOutput.has_value() ==
        synchronization.projectMpegTsOutput.has_value()) {
        return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized output requires exactly one protocol authority"));
    }
    if (synchronization.rtpOutput) {
        if (!plannedOutput.videoOutput.scheduledPacketization ||
            !plannedOutput.audioOutput.scheduledPacketization ||
            !plannedOutput.audioOutput.scheduledPacketization
                 ->maximumAccessUnitSamples()) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
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
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output does not publish audio batch timing"));
        }
        facts.protocolBatchSamples =
            program->maximumAudioAccessUnitSamples;
    }
    if (copyBounds) {
        if (!facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate != output.sampleRate() ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit !=
                copyBounds->accessUnitSamples ||
            !facts.protocolBatchSamples ||
            *facts.protocolBatchSamples != copyBounds->accessUnitSamples) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
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
    return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::success(std::move(facts));
}

} // namespace media::ffmpeg::graph

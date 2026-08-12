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
    const auto& bounds = componentBounds;
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
        if (!audio.selectedDecoder ||
            audio.selectedDecoder->inputSampleRate <= 0 ||
            audio.selectedDecoder->maximumOutputBlockInputSamples <= 0 ||
            audio.selectedDecoder->maximumOutputBlockInputSamples >
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
        facts.inputAudioSampleRate =
            audio.selectedDecoder->inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            audio.selectedDecoder->maximumOutputBlockInputSamples);
        facts.inputVideoPacketDuration =
            selectedProgram->videoPacketDuration;
        facts.inputAudioPacketDuration =
            selectedProgram->audioPacketDuration;
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (!synchronization.demuxTimestampInput ||
            !audio.selectedDecoder ||
            audio.selectedDecoder->inputSampleRate <= 0 ||
            audio.selectedDecoder->maximumOutputBlockInputSamples <= 0 ||
            audio.selectedDecoder->maximumOutputBlockInputSamples >
                std::numeric_limits<std::uint32_t>::max()) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp input does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate =
            audio.selectedDecoder->inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            audio.selectedDecoder->maximumOutputBlockInputSamples);
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
    facts.mailboxDeliveryMarginSamples = bounds.mailboxDeliveryMarginSamples;
    facts.maximumResamplerOutputBlockSamples = bounds.maximumResamplerOutputBlockSamples;
    facts.mailboxCapacity = bounds.mailboxCapacity;
    facts.acknowledgementTimeout = synchronization.recovery.reacquisitionTimeoutNs;
    facts.terminalDrainWindow = synchronization.audioServo.maximumMeasurementGapNs;
    return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::success(std::move(facts));
}

} // namespace media::ffmpeg::graph

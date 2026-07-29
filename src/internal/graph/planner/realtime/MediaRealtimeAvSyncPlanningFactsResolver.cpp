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
    const MediaRealtimeRtpTranscodePlan& plan,
    const MediaRealtimeOutputPlanningDraft& plannedOutput,
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
            !plan.audioInput.rtpDepacketizer ||
            plan.audioInput.rtpDepacketizer->accessUnitDurationRtpTicks <= 0) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "RTP input clock does not publish complete duration facts"));
        }
        facts.inputVideoClockRate =
            *synchronization.rtpInput->videoInput.clockRate;
        facts.inputAudioSampleRate =
            *synchronization.rtpInput->audioInput.clockRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            plan.audioInput.rtpDepacketizer->accessUnitDurationRtpTicks);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        if (!plan.audioPlan.selectedDecoder ||
            plan.audioPlan.selectedDecoder->inputSampleRate <= 0 ||
            plan.audioPlan.selectedDecoder->maximumOutputBlockInputSamples <= 0 ||
            plan.audioPlan.selectedDecoder->maximumOutputBlockInputSamples >
                std::numeric_limits<std::uint32_t>::max() ||
            !synchronization.mpegTsInput ||
            !synchronization.mpegTsInput->videoPid ||
            !synchronization.mpegTsInput->audioPid ||
            !plan.input.mpegTs ||
            !plan.input.mpegTs->videoPacketDuration ||
            !plan.input.mpegTs->audioPacketDuration ||
            !validPacketDurationEvidence(
                *plan.input.mpegTs->videoPacketDuration,
                plan.videoPlan.sourceStreamIndex,
                *synchronization.mpegTsInput->videoPid) ||
            !validPacketDurationEvidence(
                *plan.input.mpegTs->audioPacketDuration,
                plan.audioPlan.sourceStreamIndex,
                *synchronization.mpegTsInput->audioPid)) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS input clock does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate =
            plan.audioPlan.selectedDecoder->inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            plan.audioPlan.selectedDecoder->maximumOutputBlockInputSamples);
        facts.inputVideoPacketDuration =
            plan.input.mpegTs->videoPacketDuration;
        facts.inputAudioPacketDuration =
            plan.input.mpegTs->audioPacketDuration;
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (!synchronization.demuxTimestampInput ||
            !plan.audioPlan.selectedDecoder ||
            plan.audioPlan.selectedDecoder->inputSampleRate <= 0 ||
            plan.audioPlan.selectedDecoder->maximumOutputBlockInputSamples <= 0 ||
            plan.audioPlan.selectedDecoder->maximumOutputBlockInputSamples >
                std::numeric_limits<std::uint32_t>::max()) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp input does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate =
            plan.audioPlan.selectedDecoder->inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            plan.audioPlan.selectedDecoder->maximumOutputBlockInputSamples);
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
        facts.protocolBatchSamples =
            *plannedOutput.audioOutput.scheduledPacketization
                 ->maximumAccessUnitSamples();
    } else {
        if (!synchronization.projectMpegTsOutput->outputMux ||
            synchronization.projectMpegTsOutput->outputMux->parameters()
                    .maximumAudioAccessUnitSamples <= 0) {
            return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output does not publish audio batch timing"));
        }
        facts.protocolBatchSamples =
            synchronization.projectMpegTsOutput->outputMux->parameters()
                .maximumAudioAccessUnitSamples;
    }
    facts.mailboxDeliveryMarginSamples = bounds.mailboxDeliveryMarginSamples;
    facts.maximumResamplerOutputBlockSamples = bounds.maximumResamplerOutputBlockSamples;
    facts.mailboxCapacity = bounds.mailboxCapacity;
    facts.acknowledgementTimeout = synchronization.recovery.reacquisitionTimeoutNs;
    facts.terminalDrainWindow = synchronization.audioServo.maximumMeasurementGapNs;
    return ::media::Result<MediaRealtimeAvSyncPlanningFacts>::success(std::move(facts));
}

} // namespace media::ffmpeg::graph

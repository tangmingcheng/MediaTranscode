#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeInputValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <string>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalidInput(const char* field)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            std::string("Invalid synchronized input product: ") + field));
}

::media::Status validateRtpInput(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    if (outer.inputType != RealtimeInputType::RtpPort ||
        outer.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
        !runtime.synchronization.rtpInput ||
        !runtime.synchronization.startup.allowDegradedClock ||
        !outer.input.rtpTransport || !outer.audioInput.rtpTransport ||
        !std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
            runtime.assembly.inputClock) ||
        !std::holds_alternative<MediaRtpTimestampDeltaDurationPlan>(
            runtime.assembly.video.duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            runtime.assembly.audio.duration)) {
        return invalidInput("RTP clock assembly");
    }

    const auto& input = *runtime.synchronization.rtpInput;
    const auto& policy = input.input;
    constexpr std::int64_t Millisecond = 1'000'000;
    const bool transportPolicyMatches =
        policy.requireSenderReports &&
        policy.rtcpCompositionMode &&
        policy.senderReportTimeoutNs &&
        policy.identityEvidenceTimeoutNs &&
        policy.clockLossPolicy &&
        policy.secondaryClockLossPolicy &&
        input.videoInput.payloadType &&
        input.videoInput.clockRate &&
        input.audioInput.payloadType &&
        input.audioInput.clockRate &&
        policy.senderReportTimeoutNs->nanoseconds() % Millisecond == 0 &&
        policy.identityEvidenceTimeoutNs->nanoseconds() % Millisecond == 0 &&
        outer.input.rtpTransport->payloadType ==
            *input.videoInput.payloadType &&
        outer.input.rtpTransport->clockRate ==
            *input.videoInput.clockRate &&
        outer.audioInput.rtpTransport->payloadType ==
            *input.audioInput.payloadType &&
        outer.audioInput.rtpTransport->clockRate ==
            *input.audioInput.clockRate &&
        outer.input.rtpTransport->requireSenderReports ==
            *policy.requireSenderReports &&
        outer.audioInput.rtpTransport->requireSenderReports ==
            *policy.requireSenderReports &&
        !outer.input.rtpTransport->requireCname &&
        !outer.audioInput.rtpTransport->requireCname &&
        outer.input.rtpTransport->senderReportTimeoutMs ==
            policy.senderReportTimeoutNs->nanoseconds() / Millisecond &&
        outer.audioInput.rtpTransport->senderReportTimeoutMs ==
            policy.senderReportTimeoutNs->nanoseconds() / Millisecond &&
        outer.input.rtpTransport->cnameTimeoutMs ==
            policy.identityEvidenceTimeoutNs->nanoseconds() / Millisecond &&
        outer.audioInput.rtpTransport->cnameTimeoutMs ==
            policy.identityEvidenceTimeoutNs->nanoseconds() / Millisecond &&
        outer.input.rtpTransport->clockLossPolicy ==
            *policy.clockLossPolicy &&
        outer.audioInput.rtpTransport->clockLossPolicy ==
            *policy.secondaryClockLossPolicy &&
        *policy.clockLossPolicy ==
            (*runtime.synchronization.startup.allowDegradedClock
                 ? MediaRtpClockLossPolicy::FailOnExpired
                 : MediaRtpClockLossPolicy::FailOnDegraded) &&
        *policy.secondaryClockLossPolicy ==
            MediaRtpClockLossPolicy::FailOnExpired &&
        outer.input.rtpTransport->rtcpCompositionMode ==
            policy.rtcpCompositionMode &&
        outer.audioInput.rtpTransport->rtcpCompositionMode ==
            policy.rtcpCompositionMode;
    if (!transportPolicyMatches) {
        return invalidInput("RTP transport and synchronization facts");
    }

    const auto& clock =
        std::get<MediaRtpInputClockAssemblyPlan>(
            runtime.assembly.inputClock);
    const auto& videoDuration =
        std::get<MediaRtpTimestampDeltaDurationPlan>(
            runtime.assembly.video.duration);
    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            runtime.assembly.audio.duration);
    if (clock.commonEpochPolicy != policy.commonEpochPolicy ||
        policy.commonEpochPolicy !=
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime ||
        !runtime.planningFacts.inputVideoClockRate ||
        videoDuration.clockRate <= 0 ||
        videoDuration.clockRate !=
            *runtime.planningFacts.inputVideoClockRate ||
        videoDuration.clockRate != *input.videoInput.clockRate ||
        videoDuration.terminalPolicy !=
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta ||
        !runtime.planningFacts.inputAudioSampleRate ||
        audioDuration.sampleRate <= 0 ||
        audioDuration.sampleRate !=
            *runtime.planningFacts.inputAudioSampleRate ||
        audioDuration.sampleRate != *input.audioInput.clockRate ||
        !runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
        audioDuration.samplesPerAccessUnit == 0 ||
        audioDuration.samplesPerAccessUnit !=
            *runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
        (outer.videoPlan.inputCodecName != "h264" &&
         outer.videoPlan.inputCodecName != "hevc")) {
        return invalidInput("RTP duration and planning facts");
    }
    return ::media::Status::success();
}

::media::Status validateMpegTsInput(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    if (outer.inputType != RealtimeInputType::MpegTsUdp ||
        outer.inputLayout !=
            RealtimeInputStreamLayout::MuxedTransportStream ||
        !runtime.synchronization.mpegTsInput ||
        !std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
            runtime.assembly.inputClock) ||
        !std::holds_alternative<MediaPacketDurationPlan>(
            runtime.assembly.video.duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            runtime.assembly.audio.duration) ||
        !std::get<MediaPacketDurationPlan>(
            runtime.assembly.video.duration).requirePositiveDuration ||
        runtime.planningFacts.inputVideoClockRate ||
        !runtime.planningFacts.inputAudioSampleRate ||
        *runtime.planningFacts.inputAudioSampleRate <= 0 ||
        !runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
        *runtime.planningFacts.inputAudioSamplesPerAccessUnit == 0 ||
        !outer.audioPlan.selectedDecoder ||
        outer.audioPlan.selectedDecoder->inputSampleRate !=
            *runtime.planningFacts.inputAudioSampleRate ||
        outer.audioPlan.selectedDecoder->maximumOutputBlockInputSamples !=
            *runtime.planningFacts.inputAudioSamplesPerAccessUnit) {
        return invalidInput("MPEG-TS clock assembly and selected decoder");
    }

    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            runtime.assembly.audio.duration);
    const auto* selectedProgram = outer.input.mpegTs
        ? std::get_if<MediaTsAudioVideoSelectedProgramPlan>(
              &outer.input.mpegTs->selectedProgram)
        : nullptr;
    if (audioDuration.sampleRate !=
            *runtime.planningFacts.inputAudioSampleRate ||
        audioDuration.samplesPerAccessUnit !=
            *runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
        !outer.input.mpegTs ||
        outer.input.mpegTs->initialSourceGeneration !=
            MediaFirstLockedSourceGeneration ||
        !selectedProgram ||
        !runtime.planningFacts.inputVideoPacketDuration ||
        !runtime.planningFacts.inputAudioPacketDuration ||
        runtime.planningFacts.inputVideoPacketDuration !=
            selectedProgram->videoPacketDuration ||
        runtime.planningFacts.inputAudioPacketDuration !=
            selectedProgram->audioPacketDuration ||
        runtime.planningFacts.inputVideoPacketDuration->packetDuration <= 0 ||
        runtime.planningFacts.inputAudioPacketDuration->packetDuration <= 0 ||
        runtime.planningFacts.inputVideoPacketDuration->timeBase.num <= 0 ||
        runtime.planningFacts.inputVideoPacketDuration->timeBase.den <= 0 ||
        runtime.planningFacts.inputAudioPacketDuration->timeBase.num <= 0 ||
        runtime.planningFacts.inputAudioPacketDuration->timeBase.den <= 0) {
        return invalidInput("MPEG-TS duration evidence");
    }
    return ::media::Status::success();
}

::media::Status validateDemuxInput(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    if (outer.inputType != RealtimeInputType::Url ||
        outer.inputLayout != RealtimeInputStreamLayout::SessionDescribed ||
        !runtime.synchronization.demuxTimestampInput ||
        !std::holds_alternative<MediaDemuxTimestampInputClockAssemblyPlan>(
            runtime.assembly.inputClock) ||
        !std::holds_alternative<MediaPacketDurationPlan>(
            runtime.assembly.video.duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            runtime.assembly.audio.duration) ||
        !std::get<MediaPacketDurationPlan>(
            runtime.assembly.video.duration).requirePositiveDuration ||
        !runtime.planningFacts.inputAudioSampleRate ||
        *runtime.planningFacts.inputAudioSampleRate <= 0 ||
        !runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
        *runtime.planningFacts.inputAudioSamplesPerAccessUnit == 0 ||
        !outer.audioPlan.selectedDecoder ||
        outer.audioPlan.selectedDecoder->inputSampleRate !=
            *runtime.planningFacts.inputAudioSampleRate ||
        outer.audioPlan.selectedDecoder->maximumOutputBlockInputSamples !=
            *runtime.planningFacts.inputAudioSamplesPerAccessUnit) {
        return invalidInput("demux timestamp clock assembly");
    }
    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            runtime.assembly.audio.duration);
    if (audioDuration.sampleRate !=
            *runtime.planningFacts.inputAudioSampleRate ||
        audioDuration.samplesPerAccessUnit !=
            *runtime.planningFacts.inputAudioSamplesPerAccessUnit) {
        return invalidInput("demux timestamp audio duration authority");
    }
    const auto& input =
        *runtime.synchronization.demuxTimestampInput;
    const auto& selected =
        std::get<MediaDemuxTimestampInputClockAssemblyPlan>(
            runtime.assembly.inputClock);
    if (!input.firstWindowMaximumSkewNs ||
        !input.discontinuityThresholdNs || !input.initialGeneration ||
        !input.canonicalTargetEpochNs ||
        selected.videoTimeBase.num != input.videoTimeBase.num ||
        selected.videoTimeBase.den != input.videoTimeBase.den ||
        selected.audioTimeBase.num != input.audioTimeBase.num ||
        selected.audioTimeBase.den != input.audioTimeBase.den ||
        selected.firstWindowMaximumSkew !=
            *input.firstWindowMaximumSkewNs ||
        selected.discontinuityThreshold !=
            *input.discontinuityThresholdNs ||
        selected.initialGeneration != *input.initialGeneration ||
        selected.videoSourceIdentity != runtime.assembly.video.sourceIdentity ||
        selected.audioSourceIdentity != runtime.assembly.audio.sourceIdentity ||
        selected.canonicalTargetEpoch !=
            *input.canonicalTargetEpochNs) {
        return invalidInput("demux timestamp policy");
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRealtimeAvSyncRuntimeInputValidator::validate(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    if (!runtime.synchronization.sourceClockMode) {
        return invalidInput("source clock mode");
    }
    switch (*runtime.synchronization.sourceClockMode) {
    case MediaAvSyncSourceClockMode::RtpSenderReports:
        return validateRtpInput(outer, runtime);
    case MediaAvSyncSourceClockMode::MpegTsPcr:
        return validateMpegTsInput(outer, runtime);
    case MediaAvSyncSourceClockMode::DemuxTimestamps:
        return validateDemuxInput(outer, runtime);
    }
    return invalidInput("unsupported source clock mode");
}

} // namespace media::ffmpeg::graph

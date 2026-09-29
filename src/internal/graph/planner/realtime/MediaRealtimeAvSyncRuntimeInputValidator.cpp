#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeInputValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"

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
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyPlan& assembly)
{
    if (request.inputType != RealtimeInputType::RtpPort ||
        request.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
        !synchronization.rtpInput ||
        !synchronization.startup.allowDegradedClock ||
        !request.input.rtpTransport || !request.isolatedAudioInput ||
        !request.isolatedAudioInput->rtpTransport ||
        !std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
            assembly.inputClock) ||
        !std::holds_alternative<MediaRtpTimestampDeltaDurationPlan>(
            assembly.video.duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio.duration)) {
        return invalidInput("RTP clock assembly");
    }

    const auto& input = *synchronization.rtpInput;
    const auto& policy = input.input;
    constexpr std::int64_t Millisecond = 1'000'000;
    const bool preserve = synchronization.sourceLifecycle &&
        synchronization.sourceLifecycle->mode ==
            MediaAvSourceLifecycleMode::PreserveActivatedOutput;
    const bool transportPolicyMatches =
        policy.requireSenderReports &&
        policy.rtcpCompositionMode &&
        policy.senderReportTimeoutNs &&
        policy.maximumExtrapolationNs &&
        policy.identityEvidenceTimeoutNs &&
        policy.clockLossPolicy &&
        policy.secondaryClockLossPolicy &&
        input.videoInput.payloadType &&
        input.videoInput.clockRate &&
        input.audioInput.payloadType &&
        input.audioInput.clockRate &&
        policy.senderReportTimeoutNs->nanoseconds() % Millisecond == 0 &&
        policy.maximumExtrapolationNs->nanoseconds() % Millisecond == 0 &&
        policy.identityEvidenceTimeoutNs->nanoseconds() % Millisecond == 0 &&
        request.input.rtpTransport->payloadType ==
            *input.videoInput.payloadType &&
        request.input.rtpTransport->clockRate ==
            *input.videoInput.clockRate &&
        request.isolatedAudioInput->rtpTransport->payloadType ==
            *input.audioInput.payloadType &&
        request.isolatedAudioInput->rtpTransport->clockRate ==
            *input.audioInput.clockRate &&
        request.input.rtpTransport->requireSenderReports ==
            *policy.requireSenderReports &&
        request.isolatedAudioInput->rtpTransport->requireSenderReports ==
            *policy.requireSenderReports &&
        !request.input.rtpTransport->requireCname &&
        !request.isolatedAudioInput->rtpTransport->requireCname &&
        request.input.rtpTransport->senderReportTimeoutMs ==
            policy.senderReportTimeoutNs->nanoseconds() / Millisecond &&
        request.isolatedAudioInput->rtpTransport->senderReportTimeoutMs ==
            policy.senderReportTimeoutNs->nanoseconds() / Millisecond &&
        request.input.rtpTransport->maximumExtrapolationMs ==
            policy.maximumExtrapolationNs->nanoseconds() / Millisecond &&
        request.isolatedAudioInput->rtpTransport->maximumExtrapolationMs ==
            policy.maximumExtrapolationNs->nanoseconds() / Millisecond &&
        request.input.rtpTransport->cnameTimeoutMs ==
            policy.identityEvidenceTimeoutNs->nanoseconds() / Millisecond &&
        request.isolatedAudioInput->rtpTransport->cnameTimeoutMs ==
            policy.identityEvidenceTimeoutNs->nanoseconds() / Millisecond &&
        request.input.rtpTransport->clockLossPolicy ==
            *policy.clockLossPolicy &&
        request.isolatedAudioInput->rtpTransport->clockLossPolicy ==
            *policy.secondaryClockLossPolicy &&
        *policy.clockLossPolicy ==
            (preserve ? MediaRtpClockLossPolicy::InvalidateAndWait :
             *synchronization.startup.allowDegradedClock
                 ? MediaRtpClockLossPolicy::FailOnExpired
                 : MediaRtpClockLossPolicy::FailOnDegraded) &&
        *policy.secondaryClockLossPolicy ==
            (preserve ? MediaRtpClockLossPolicy::InvalidateAndWait
                      : MediaRtpClockLossPolicy::FailOnExpired) &&
        request.input.rtpTransport->rtcpCompositionMode ==
            policy.rtcpCompositionMode &&
        request.isolatedAudioInput->rtpTransport->rtcpCompositionMode ==
            policy.rtcpCompositionMode;
    if (!transportPolicyMatches) {
        return invalidInput("RTP transport and synchronization facts");
    }

    const auto& clock =
        std::get<MediaRtpInputClockAssemblyPlan>(
            assembly.inputClock);
    const auto& videoDuration =
        std::get<MediaRtpTimestampDeltaDurationPlan>(
            assembly.video.duration);
    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio.duration);
    if (clock.commonEpochPolicy != policy.commonEpochPolicy ||
        policy.commonEpochPolicy !=
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime ||
        !facts.inputVideoClockRate ||
        videoDuration.clockRate <= 0 ||
        videoDuration.clockRate !=
            *facts.inputVideoClockRate ||
        videoDuration.clockRate != *input.videoInput.clockRate ||
        videoDuration.terminalPolicy !=
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta ||
        !facts.inputAudioSampleRate ||
        audioDuration.sampleRate <= 0 ||
        audioDuration.sampleRate !=
            *facts.inputAudioSampleRate ||
        audioDuration.sampleRate != *input.audioInput.clockRate ||
        !facts.inputAudioSamplesPerAccessUnit ||
        audioDuration.samplesPerAccessUnit == 0 ||
        audioDuration.samplesPerAccessUnit !=
            *facts.inputAudioSamplesPerAccessUnit ||
        (request.videoCodecName != "h264" &&
         request.videoCodecName != "hevc")) {
        return invalidInput("RTP duration and planning facts");
    }
    return ::media::Status::success();
}

::media::Status validateMpegTsInput(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyPlan& assembly)
{
    const bool transcodeInputMatches =
        request.audio.branchMode == MediaBranchMode::TranscodeFrame &&
        facts.inputAudioSampleRate &&
        facts.inputAudioSamplesPerAccessUnit &&
        request.audio.selectedDecoder &&
        request.audio.selectedDecoder->inputSampleRate ==
            *facts.inputAudioSampleRate &&
        request.audio.selectedDecoder->maximumOutputBlockInputSamples ==
            *facts.inputAudioSamplesPerAccessUnit;
    const bool copyInputMatches =
        request.audio.branchMode == MediaBranchMode::CopyPacket &&
        facts.inputAudioSampleRate &&
        facts.inputAudioSamplesPerAccessUnit &&
        !request.audio.selectedDecoder &&
        !request.audio.selectedResampler &&
        request.audio.resolvedOutput &&
        request.audio.maximumAccessUnitSamples &&
        request.audio.resolvedOutput->sampleRate() ==
            *facts.inputAudioSampleRate &&
        *request.audio.maximumAccessUnitSamples ==
            *facts.inputAudioSamplesPerAccessUnit;
    if (request.inputType != RealtimeInputType::MpegTsUdp ||
        request.inputLayout !=
            RealtimeInputStreamLayout::MuxedTransportStream ||
        !synchronization.mpegTsInput ||
        !std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
            assembly.inputClock) ||
        !std::holds_alternative<MediaPacketDurationPlan>(
            assembly.video.duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio.duration) ||
        !std::get<MediaPacketDurationPlan>(
            assembly.video.duration).requirePositiveDuration ||
        facts.inputVideoClockRate ||
        !facts.inputAudioSampleRate ||
        *facts.inputAudioSampleRate <= 0 ||
        !facts.inputAudioSamplesPerAccessUnit ||
        *facts.inputAudioSamplesPerAccessUnit == 0 ||
        (!transcodeInputMatches && !copyInputMatches)) {
        return invalidInput("MPEG-TS clock assembly and selected decoder");
    }

    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio.duration);
    const auto* selectedProgram = request.input.mpegTs
        ? std::get_if<MediaTsAudioVideoSelectedProgramPlan>(
              &request.input.mpegTs->selectedProgram)
        : nullptr;
    if (audioDuration.sampleRate !=
            *facts.inputAudioSampleRate ||
        audioDuration.samplesPerAccessUnit !=
            *facts.inputAudioSamplesPerAccessUnit ||
        !request.input.mpegTs ||
        request.input.mpegTs->initialSourceGeneration !=
            MediaFirstLockedSourceGeneration ||
        !selectedProgram ||
        !facts.inputVideoPacketDuration ||
        !facts.inputAudioPacketDuration ||
        facts.inputVideoPacketDuration !=
            selectedProgram->videoPacketDuration ||
        facts.inputAudioPacketDuration !=
            selectedProgram->audioPacketDuration ||
        facts.inputVideoPacketDuration->packetDuration <= 0 ||
        facts.inputAudioPacketDuration->packetDuration <= 0 ||
        facts.inputVideoPacketDuration->timeBase.num <= 0 ||
        facts.inputVideoPacketDuration->timeBase.den <= 0 ||
        facts.inputAudioPacketDuration->timeBase.num <= 0 ||
        facts.inputAudioPacketDuration->timeBase.den <= 0) {
        return invalidInput("MPEG-TS duration evidence");
    }
    return ::media::Status::success();
}

::media::Status validateDemuxInput(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyPlan& assembly)
{
    const bool transcodeInputMatches =
        request.audio.branchMode == MediaBranchMode::TranscodeFrame &&
        facts.inputAudioSampleRate &&
        facts.inputAudioSamplesPerAccessUnit &&
        request.audio.selectedDecoder &&
        request.audio.selectedDecoder->inputSampleRate ==
            *facts.inputAudioSampleRate &&
        request.audio.selectedDecoder->maximumOutputBlockInputSamples ==
            *facts.inputAudioSamplesPerAccessUnit;
    const bool copyInputMatches =
        request.audio.branchMode == MediaBranchMode::CopyPacket &&
        facts.inputAudioSampleRate &&
        facts.inputAudioSamplesPerAccessUnit &&
        !request.audio.selectedDecoder &&
        !request.audio.selectedResampler &&
        request.audio.resolvedOutput &&
        request.audio.maximumAccessUnitSamples &&
        request.audio.resolvedOutput->sampleRate() ==
            *facts.inputAudioSampleRate &&
        *request.audio.maximumAccessUnitSamples ==
            *facts.inputAudioSamplesPerAccessUnit;
    if (request.inputType != RealtimeInputType::Url ||
        request.inputLayout != RealtimeInputStreamLayout::SessionDescribed ||
        !synchronization.demuxTimestampInput ||
        !std::holds_alternative<MediaDemuxTimestampInputClockAssemblyPlan>(
            assembly.inputClock) ||
        !std::holds_alternative<MediaPacketDurationPlan>(
            assembly.video.duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio.duration) ||
        !std::get<MediaPacketDurationPlan>(
            assembly.video.duration).requirePositiveDuration ||
        !facts.inputAudioSampleRate ||
        *facts.inputAudioSampleRate <= 0 ||
        !facts.inputAudioSamplesPerAccessUnit ||
        *facts.inputAudioSamplesPerAccessUnit == 0 ||
        (!transcodeInputMatches && !copyInputMatches)) {
        return invalidInput("demux timestamp clock assembly");
    }
    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio.duration);
    if (audioDuration.sampleRate !=
            *facts.inputAudioSampleRate ||
        audioDuration.samplesPerAccessUnit !=
            *facts.inputAudioSamplesPerAccessUnit) {
        return invalidInput("demux timestamp audio duration authority");
    }
    const auto& input =
        *synchronization.demuxTimestampInput;
    const auto& selected =
        std::get<MediaDemuxTimestampInputClockAssemblyPlan>(
            assembly.inputClock);
    if (!input.firstWindowMaximumSkewNs ||
        !input.discontinuityThresholdNs || !input.initialGeneration ||
        !input.canonicalTargetEpochNs || !input.preparedInput ||
        !input.preparedEvidence ||
        selected.videoTimeBase.num != input.videoTimeBase.num ||
        selected.videoTimeBase.den != input.videoTimeBase.den ||
        selected.audioTimeBase.num != input.audioTimeBase.num ||
        selected.audioTimeBase.den != input.audioTimeBase.den ||
        selected.firstWindowMaximumSkew !=
            *input.firstWindowMaximumSkewNs ||
        selected.discontinuityThreshold !=
            *input.discontinuityThresholdNs ||
        selected.initialGeneration != *input.initialGeneration ||
        selected.videoSourceIdentity != assembly.video.sourceIdentity ||
        selected.audioSourceIdentity != assembly.audio.sourceIdentity ||
        selected.canonicalTargetEpoch !=
            *input.canonicalTargetEpochNs ||
        selected.preparedInput != *input.preparedInput ||
        selected.preparedEvidence != *input.preparedEvidence ||
        synchronization.startup.requireVideoKeyFrame != true ||
        input.preparedInput->leadingVideoDisposition !=
            MediaPreparedLeadingVideoDisposition::
                DiscardUntimedNonKeyBeforeFirstTimedVideo ||
        input.preparedInput->timedStartupPrefixDisposition !=
            MediaPreparedTimedStartupPrefixDisposition::
                DiscardEarlierCompleteTimedUntilCommonWindow) {
        return invalidInput("demux timestamp policy");
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRealtimeAvSyncRuntimeInputValidator::validate(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyPlan& assembly)
{
    if (!synchronization.sourceClockMode) {
        return invalidInput("source clock mode");
    }
    switch (*synchronization.sourceClockMode) {
    case MediaAvSyncSourceClockMode::RtpSenderReports:
        return validateRtpInput(request, synchronization, facts, assembly);
    case MediaAvSyncSourceClockMode::MpegTsPcr:
        if (request.isolatedAudioInput) {
            return invalidInput("MPEG-TS rejects isolated audio input");
        }
        return validateMpegTsInput(request, synchronization, facts, assembly);
    case MediaAvSyncSourceClockMode::DemuxTimestamps:
        if (request.isolatedAudioInput) {
            return invalidInput("demux input rejects isolated audio input");
        }
        return validateDemuxInput(request, synchronization, facts, assembly);
    }
    return invalidInput("unsupported source clock mode");
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeInputValidator.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"

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
    const MediaAvSyncPlanView& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyView& assembly)
{
    const auto* canonicalVideo = std::get_if<MediaCanonicalVideoAssemblyTiming>(
        &assembly.video.timing);
    const bool decoded = std::holds_alternative<MediaDecodedVideoAssemblyTiming>(assembly.video.timing);
    if (decoded ? synchronization.startup.videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan ||
                      synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution
                : !canonicalVideo || synchronization.startup.videoReadiness != MediaVideoStartupReadiness::IntervalCoverage)
        return invalidInput("RTP video assembly and startup readiness");

    const bool hasAudio = synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    const bool videoSource = synchronization.members == MediaTranscodeStreamSet::VideoOnly &&
        synchronization.domainRole == MediaAvSyncDomainRole::SourceContribution;
    if ((!hasAudio && !videoSource) || request.audio.enabled != hasAudio ||
        assembly.audio.has_value() != hasAudio || (request.isolatedAudioInput != nullptr) != hasAudio ||
        request.inputType != RealtimeInputType::RtpPort ||
        request.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
        !synchronization.rtpInput || !synchronization.startup.allowDegradedClock ||
        !std::holds_alternative<MediaRtpInputClockAssemblyPlan>(assembly.inputClock) ||
        (!decoded && (!std::holds_alternative<MediaRtpTimestampDeltaDurationPlan>(canonicalVideo->duration) ||
                      canonicalVideo->decodeOrder != MediaDecodeOrderMode::ReorderedRequiresDecodeTime)) ||
        (hasAudio && !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(assembly.audio->duration)))
        return invalidInput("RTP clock assembly and source members");

    const auto& input = *synchronization.rtpInput;
    const auto& policy = input.input;
    constexpr std::int64_t Millisecond = 1'000'000;
    const bool preserve = synchronization.sourceLifecycle &&
        synchronization.sourceLifecycle->mode == MediaAvSourceLifecycleMode::PreserveActivatedOutput;
    if (!policy.requireSenderReports || !policy.rtcpCompositionMode ||
        !policy.senderReportTimeoutNs || !policy.maximumExtrapolationNs ||
        !policy.identityEvidenceTimeoutNs || !policy.clockLossPolicy || !policy.secondaryClockLossPolicy ||
        policy.senderReportTimeoutNs->nanoseconds() % Millisecond != 0 ||
        policy.maximumExtrapolationNs->nanoseconds() % Millisecond != 0 ||
        policy.identityEvidenceTimeoutNs->nanoseconds() % Millisecond != 0 ||
        *policy.clockLossPolicy != (preserve ? MediaRtpClockLossPolicy::InvalidateAndWait :
            *synchronization.startup.allowDegradedClock ? MediaRtpClockLossPolicy::FailOnExpired :
                                                        MediaRtpClockLossPolicy::FailOnDegraded) ||
        *policy.secondaryClockLossPolicy != (preserve ? MediaRtpClockLossPolicy::InvalidateAndWait :
                                                        MediaRtpClockLossPolicy::FailOnExpired))
        return invalidInput("RTP clock transport policy");
    const auto transportMatches = [&](const MediaRealtimeRtpInputNodePlan& node,
                                      const MediaAvSyncRtpInputStreamPlan& stream,
                                      MediaRtpClockLossPolicy loss) {
        if (!node.rtpTransport || !stream.payloadType || !stream.clockRate) return false;
        const auto& transport = *node.rtpTransport;
        return transport.payloadType == *stream.payloadType && transport.clockRate == *stream.clockRate &&
            transport.requireSenderReports == *policy.requireSenderReports && !transport.requireCname &&
            transport.senderReportTimeoutMs == policy.senderReportTimeoutNs->nanoseconds() / Millisecond &&
            transport.maximumExtrapolationMs == policy.maximumExtrapolationNs->nanoseconds() / Millisecond &&
            transport.cnameTimeoutMs == policy.identityEvidenceTimeoutNs->nanoseconds() / Millisecond &&
            transport.clockLossPolicy == loss && transport.rtcpCompositionMode == policy.rtcpCompositionMode;
    };
    if (!transportMatches(request.input, input.videoInput, *policy.clockLossPolicy) ||
        (hasAudio && !transportMatches(*request.isolatedAudioInput, input.audioInput, *policy.secondaryClockLossPolicy)))
        return invalidInput("RTP transport and synchronization facts");

    const auto& clock = std::get<MediaRtpInputClockAssemblyPlan>(assembly.inputClock);
    if (clock.commonEpochPolicy != policy.commonEpochPolicy ||
        policy.commonEpochPolicy != MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime ||
        !facts.inputVideoClockRate || *facts.inputVideoClockRate <= 0 ||
        *facts.inputVideoClockRate != *input.videoInput.clockRate ||
        !input.videoInput.identity || !synchronization.startup.videoIdentity ||
        assembly.video.sourceIdentity.empty() ||
        assembly.video.sourceIdentity != *input.videoInput.identity ||
        assembly.video.sourceIdentity != *synchronization.startup.videoIdentity ||
        facts.inputVideoIdentity != synchronization.startup.videoIdentity ||
        (request.videoCodecName != "h264" && request.videoCodecName != "hevc"))
        return invalidInput("RTP video clock, identity and planning facts");
    if (canonicalVideo) {
        const auto& duration = std::get<MediaRtpTimestampDeltaDurationPlan>(canonicalVideo->duration);
        if (duration.clockRate != *facts.inputVideoClockRate ||
            duration.terminalPolicy != MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta)
            return invalidInput("RTP canonical duration authority");
    }
    if (hasAudio) {
        const auto& audioDuration = std::get<MediaPlannedAudioSamplesDurationPlan>(assembly.audio->duration);
        if (!facts.inputAudioSampleRate || !facts.inputAudioSamplesPerAccessUnit ||
            audioDuration.sampleRate <= 0 || audioDuration.sampleRate != *facts.inputAudioSampleRate ||
            audioDuration.sampleRate != *input.audioInput.clockRate || audioDuration.samplesPerAccessUnit == 0 ||
            audioDuration.samplesPerAccessUnit != *facts.inputAudioSamplesPerAccessUnit)
            return invalidInput("RTP audio duration and planning facts");
    } else if (facts.inputAudioIdentity || facts.inputAudioSampleRate || facts.inputAudioSamplesPerAccessUnit ||
               facts.inputAudioPacketDuration || input.audioInput.identity ||
               input.audioInput.payloadType || input.audioInput.clockRate) {
        return invalidInput("Video-only RTP contains audio timing facts");
    }
    return ::media::Status::success();
}

::media::Status validateMpegTsInput(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlanView& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyView& assembly)
{
    const auto* canonicalVideo = std::get_if<MediaCanonicalVideoAssemblyTiming>(
        &assembly.video.timing);
    if (!canonicalVideo) return invalidInput("canonical packet input requires interval assembly");

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
    if (!assembly.audio || request.inputType != RealtimeInputType::MpegTsUdp ||
        request.inputLayout !=
            RealtimeInputStreamLayout::MuxedTransportStream ||
        !synchronization.mpegTsInput ||
        !std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
            assembly.inputClock) ||
        !std::holds_alternative<MediaPacketDurationPlan>(
            canonicalVideo->duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio->duration) ||
        !std::get<MediaPacketDurationPlan>(
            canonicalVideo->duration).requirePositiveDuration ||
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
            assembly.audio->duration);
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
    const MediaAvSyncPlanView& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyView& assembly)
{
    const auto* canonicalVideo = std::get_if<MediaCanonicalVideoAssemblyTiming>(
        &assembly.video.timing);
    if (!canonicalVideo) return invalidInput("canonical packet input requires interval assembly");

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
    if (!assembly.audio || request.inputType != RealtimeInputType::Url ||
        request.inputLayout != RealtimeInputStreamLayout::SessionDescribed ||
        !synchronization.demuxTimestampInput ||
        !std::holds_alternative<MediaDemuxTimestampInputClockAssemblyPlan>(
            assembly.inputClock) ||
        !std::holds_alternative<MediaPacketDurationPlan>(
            canonicalVideo->duration) ||
        !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio->duration) ||
        !std::get<MediaPacketDurationPlan>(
            canonicalVideo->duration).requirePositiveDuration ||
        !facts.inputAudioSampleRate ||
        *facts.inputAudioSampleRate <= 0 ||
        !facts.inputAudioSamplesPerAccessUnit ||
        *facts.inputAudioSamplesPerAccessUnit == 0 ||
        (!transcodeInputMatches && !copyInputMatches)) {
        return invalidInput("demux timestamp clock assembly");
    }
    const auto& audioDuration =
        std::get<MediaPlannedAudioSamplesDurationPlan>(
            assembly.audio->duration);
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
        selected.audioSourceIdentity != assembly.audio->sourceIdentity ||
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
    const MediaAvSyncPlanView& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    const MediaRealtimeAvSyncAssemblyView& assembly)
{
    const bool decoded = synchronization.startup.videoReadiness ==
        MediaVideoStartupReadiness::DecodedPresentationSpan;
    if (decoded != (request.sourceCapacity != nullptr))
        return invalidInput("source acquiring capacity domain");
    if (const auto* capacity = request.sourceCapacity) {
        if (assembly.audio.has_value() != capacity->audioAcquiringCapacity.has_value() ||
            assembly.video.acquiringCapacity != capacity->videoAcquiringCapacity ||
            (assembly.audio && assembly.audio->acquiringCapacity != *capacity->audioAcquiringCapacity))
            return invalidInput("source acquiring capacity differs from pure facts");
    }
    if (synchronization.startup.videoReadiness != MediaVideoStartupReadiness::IntervalCoverage &&
        (synchronization.startup.videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan ||
         synchronization.sourceClockMode != MediaAvSyncSourceClockMode::RtpSenderReports ||
         synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution))
        return invalidInput("decoded startup is limited to RTP source contribution assembly");
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

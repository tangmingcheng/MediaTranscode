#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"

#include "internal/graph/planner/MediaRtpClockLivenessPolicy.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/planner/realtime/MediaTsReceiverTimingPlanner.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t Millisecond = 1'000'000;
constexpr std::int64_t Second = 1'000'000'000;

constexpr MediaRunningTime runningTime(std::int64_t nanoseconds) noexcept
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

void planSharedNonStartupPolicy(MediaAvSyncPlan& plan)
{
    plan.masterClockMode = MediaAvSyncMasterClockMode::SteadyMonotonic;
    plan.canonicalTimeBaseNumerator = 1;
    plan.canonicalTimeBaseDenominator = 1'000'000'000;

    plan.audioServo.deadbandNs = runningTime(Millisecond);
    plan.audioServo.phaseFilterTimeConstantNs = runningTime(250 * Millisecond);
    plan.audioServo.proportionalGainPpmPerSecond = 20'000;
    plan.audioServo.integralGainPpmPerSecondSquared = 1'000;
    plan.audioServo.integratorLimitPpm = 2'000;
    plan.audioServo.frequencyFeedForwardNumerator = 1;
    plan.audioServo.frequencyFeedForwardDenominator = 1;
    plan.audioServo.frequencyDeadbandPpm = 10;
    plan.audioServo.maximumMeasuredFrequencyPpm = 10'000;
    plan.audioServo.recoveryExitFrequencyPpm = 500;
    plan.audioServo.antiWindupMode =
        MediaAudioServoAntiWindupMode::ConditionalIntegration;
    plan.audioServo.minimumUpdateIntervalNs = runningTime(10 * Millisecond);
    plan.audioServo.maximumMeasurementGapNs = runningTime(Second);
    plan.audioServo.maximumSlewPpmPerSecond = 100;
    plan.audioServo.normalCorrectionLimitPpm = 1000;
    plan.audioServo.recoveryCorrectionLimitPpm = 5000;
    plan.audioServo.recoveryEnterThresholdNs = runningTime(100 * Millisecond);
    plan.audioServo.recoveryExitThresholdNs = runningTime(50 * Millisecond);
    plan.audioServo.recoveryExitHoldNs = runningTime(500 * Millisecond);
    plan.audioServo.correctionLookaheadWindows = 2;

    plan.video.earlyHoldThresholdNs = runningTime(20 * Millisecond);
    plan.video.lateDisplayThresholdNs = runningTime(40 * Millisecond);
    plan.video.dropThresholdNs = runningTime(80 * Millisecond);
    plan.video.allowRecoveryRepeat = true;
    plan.video.maximumConsecutiveRecoveryActions = 5;

    plan.recovery.suspectThresholdNs = runningTime(100 * Millisecond);
    plan.recovery.hardDiscontinuityThresholdNs = runningTime(250 * Millisecond);
    plan.recovery.reacquisitionTimeoutNs = runningTime(10 * Second);

    plan.metrics.collectStateAndGeneration = true;
    plan.metrics.collectClockEvidence = true;
    plan.metrics.collectQueueDurations = true;
    plan.metrics.collectPhaseErrors = true;
    plan.metrics.collectAudioCorrection = true;
    plan.metrics.collectVideoRecoveryCounts = true;
    plan.metrics.collectDiscontinuityCounts = true;
    plan.metrics.collectProtocolClockHealth = true;
    plan.metrics.maximumStartupSkewNs = runningTime(40 * Millisecond);
    plan.metrics.maximumSteadyP95SkewNs = runningTime(20 * Millisecond);
    plan.metrics.maximumSteadyP99SkewNs = runningTime(40 * Millisecond);
    plan.metrics.maximumDriftNsPerHour = runningTime(Millisecond);
}

::media::Result<MediaAvSyncRtpInputPlan> planRtpInput(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.videoRtp.payloadType || !request.input.videoRtp.clockRate ||
        !request.input.audioRtp.payloadType || !request.input.audioRtp.clockRate) {
        return ::media::Result<MediaAvSyncRtpInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized separate RTP requires explicit audio/video payload types and clock rates"));
    }

    MediaAvSyncRtpInputPlan input;
    const std::string& groupIdentity = request.mediaId;

    input.videoInput.identity = groupIdentity + ".input.video";
    input.videoInput.payloadType = *request.input.videoRtp.payloadType;
    input.videoInput.clockRate = *request.input.videoRtp.clockRate;
    input.audioInput.identity = groupIdentity + ".input.audio";
    input.audioInput.payloadType = *request.input.audioRtp.payloadType;
    input.audioInput.clockRate = *request.input.audioRtp.clockRate;
    input.input.streamAssociationMode =
        MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair;
    input.input.rtcpCompositionMode =
        MediaRtcpCompositionMode::ReducedSizeRfc5506;
    input.input.identityEvidenceTimeoutNs = runningTime(
        static_cast<std::int64_t>(MediaRtpClockLivenessPolicy::CnameTimeoutMs) *
        Millisecond);
    input.input.commonEpochPolicy =
        MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime;
    input.input.requireSenderReports = true;
    input.input.senderReportTimeoutNs = runningTime(
        static_cast<std::int64_t>(
            MediaRtpClockLivenessPolicy::SenderReportTimeoutMs) *
        Millisecond);
    input.input.maximumExtrapolationNs = runningTime(
        static_cast<std::int64_t>(
            MediaRtpClockLivenessPolicy::MaximumExtrapolationMs) *
        Millisecond);
    input.input.clockLossPolicy = MediaRtpClockLossPolicy::FailOnDegraded;
    input.input.secondaryClockLossPolicy = MediaRtpClockLossPolicy::FailOnExpired;
    input.input.maximumInterStreamClockOffsetSkewNs =
        runningTime(50 * Millisecond);
    input.input.maximumSenderClockRateErrorPpm = 1'000;
    input.input.maximumSenderClockResidualNs = runningTime(250 * Millisecond);
    return ::media::Result<MediaAvSyncRtpInputPlan>::success(std::move(input));
}

void planRtpOutput(MediaAvSyncPlan& plan,
                   const MediaRealtimeRtpTranscodeRequest& request,
                   int audioOutputRate)
{
    const std::string& groupIdentity = request.mediaId;
    const std::string cname =
        MediaRtpOutputIdentityPlanner::cname(groupIdentity);
    plan.rtpOutput.emplace();
    plan.rtpOutput->videoOutput.identity = groupIdentity + ".output.video";
    plan.rtpOutput->videoOutput.payloadType = 96;
    plan.rtpOutput->videoOutput.clockRate = 90'000;
    plan.rtpOutput->videoOutput.ssrc =
        MediaRtpOutputIdentityPlanner::stableFfmpegMuxSsrc(
            *plan.rtpOutput->videoOutput.identity);
    plan.rtpOutput->videoOutput.baseTimestamp =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            groupIdentity + ".video.timestamp");
    plan.rtpOutput->videoOutput.cname = cname;
    plan.rtpOutput->audioOutput.identity = groupIdentity + ".output.audio";
    plan.rtpOutput->audioOutput.payloadType = 97;
    plan.rtpOutput->audioOutput.clockRate = audioOutputRate;
    plan.rtpOutput->audioOutput.ssrc =
        MediaRtpOutputIdentityPlanner::stableFfmpegMuxSsrc(
            *plan.rtpOutput->audioOutput.identity);
    plan.rtpOutput->audioOutput.baseTimestamp =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            groupIdentity + ".audio.timestamp");
    plan.rtpOutput->audioOutput.cname = cname;
    plan.rtpOutput->output.useSharedNtpEpoch = true;
}

void planTsInput(MediaAvSyncPlan& plan,
                 const MediaRealtimeRtpTranscodeRequest& request,
                 const MediaTsAudioVideoSelectedProgramPlan& selected)
{
    const auto& program = selected.selection;
    plan.sourceClockMode = MediaAvSyncSourceClockMode::MpegTsPcr;
    plan.controlGenerationPolicy =
        MediaControlGenerationPolicy::OptionalExactWhenPresent;
    plan.mpegTsInput.emplace();
    plan.mpegTsInput->programNumber = program.programNumber;
    plan.mpegTsInput->programMapPid = program.programMapPid;
    plan.mpegTsInput->videoPid = program.video.elementaryPid;
    plan.mpegTsInput->audioPid = program.audio.elementaryPid;
    const std::string& groupIdentity = request.mediaId;
    plan.startup.videoIdentity = groupIdentity + ".pid." +
                                 std::to_string(program.video.elementaryPid);
    plan.startup.audioIdentity = groupIdentity + ".pid." +
                                 std::to_string(program.audio.elementaryPid);
    plan.mpegTsInput->pcrPid = program.pcrPid;
}

::media::Result<MediaTsMuxPlan> planTsOutput(
    const MediaAvSyncPlan& plan,
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaProjectMpegTsResolvedPipelineFacts& resolvedFacts)
{
    if (!plan.startup.outputLeadNs || !request.deployment ||
        !request.deployment->encode().receiverTiming) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires planner-owned startup timing and authoritative receiver timing capability"));
    }
    if (!request.output.transport) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires an explicit transport"));
    }
    std::uint8_t maximumPacketsPerDatagram = 0;
    if (*request.output.transport == MediaOutputTransportKind::RtpAvp ||
        *request.output.transport == MediaOutputTransportKind::UdpDatagrams) {
        const auto maximumDatagram =
            request.deployment->encode().mtu.senderMaximumPayloadBytes;
        auto packetCount = MediaTsMuxPlan::maximumPacketsPerDatagram(
            static_cast<std::size_t>(maximumDatagram),
            *request.output.transport);
        if (!packetCount) {
            return ::media::Result<MediaTsMuxPlan>::failure(
                packetCount.error());
        }
        maximumPacketsPerDatagram = packetCount.value();
    } else if (*request.output.transport !=
               MediaOutputTransportKind::UdpDatagrams) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "MPEG-TS output transport is unsupported"));
    }
    if (!request.parameters.video.frameRate.complete() ||
        !request.parameters.video.frameRate.numerator ||
        !request.parameters.video.frameRate.denominator) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS receiver timing requires prepared video cadence"));
    }
    auto audioCadence = MediaRunningTime::checkedFromTicks(
        resolvedFacts.audioOutput.codecFrameSamples(), 1,
        resolvedFacts.audioOutput.sampleRate());
    const auto& deployment = request.deployment->encode();
    auto timing = audioCadence
        ? MediaTsReceiverTimingPlanner::plan(
              deployment.receiverTiming->transportDecodeLead,
              deployment.receiverTiming->authority,
              deployment.latency.targetResidence,
              deployment.latency.maximumResidence,
              deployment.latency.maximumReleaseJitter,
              deployment.latency.releaseJitterAuthority,
              MediaRational{
                  *request.parameters.video.frameRate.numerator,
                  *request.parameters.video.frameRate.denominator},
              audioCadence.value())
        : ::media::Result<MediaMpegTsTimingPolicy>::failure(
              audioCadence.error());
    auto preroll = timing
        ? MediaTsReceiverTimingPlanner::startupEmissionPreroll(
              deployment.receiverTiming->transportDecodeLead,
              MediaRational{
                  *request.parameters.video.frameRate.numerator,
                  *request.parameters.video.frameRate.denominator},
              audioCadence.value(), timing.value())
        : ::media::Result<MediaRunningTime>::failure(timing.error());
    if (!preroll) {
        return ::media::Result<MediaTsMuxPlan>::failure(preroll.error());
    }
    auto resolvedOutput = MediaProjectMpegTsOutputPlan::createAudioVideo(
        resolvedFacts.videoCodecName, resolvedFacts.videoPacketLayout,
        resolvedFacts.audioOutput, std::move(timing).value(),
        request.deployment->encode().receiverTiming->transportDecodeLead,
        preroll.value(),
        *request.output.transport,
        maximumPacketsPerDatagram);
    if (!resolvedOutput) {
        return ::media::Result<MediaTsMuxPlan>::failure(resolvedOutput.error());
    }
    return ::media::Result<MediaTsMuxPlan>::success(
        resolvedOutput.value().muxPlan());
}

} // namespace

::media::Result<MediaAvSyncRtpInputPlan> MediaAvSyncPlanner::planRtpInputClock(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (request.mediaId.empty()) {
        return ::media::Result<MediaAvSyncRtpInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V RTP input clock requires an explicit media identity"));
    }
    return planRtpInput(request);
}

::media::Result<MediaAvSyncPlan> MediaAvSyncPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsAudioVideoSelectedProgramPlan* selectedTsProgram,
    const MediaProjectMpegTsResolvedPipelineFacts* resolvedTsFacts,
    const MediaAvSyncPreparedDemuxTimestampFacts* preparedDemuxFacts,
    const MediaRealtimeGraphResourceLedgerPlan& resourceLedger,
    MediaBranchMode audioBranchMode,
    int resolvedOutputAudioSampleRate)
{
    if (request.mediaId.empty()) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V synchronization requires an explicit media identity"));
    }
    if (request.parameters.execution.streamSet != MediaTranscodeStreamSet::AudioVideo) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported("A/V synchronization requires both audio and video"));
    }
    if (resolvedOutputAudioSampleRate <= 0) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V synchronization requires a resolved output audio sample rate"));
    }
    if (audioBranchMode != MediaBranchMode::CopyPacket &&
        audioBranchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V synchronization requires a planned audio execution branch"));
    }
    MediaAvSyncPlan plan;
    if (preparedDemuxFacts) {
        plan.startup = preparedDemuxFacts->startup;
    } else {
        auto startup = MediaAvSyncStartupPolicyPlanner::plan(
            request, resourceLedger);
        if (!startup) {
            return ::media::Result<MediaAvSyncPlan>::failure(startup.error());
        }
        plan.startup = std::move(startup).value();
    }
    plan.startup.trimAudioToCommonStart =
        audioBranchMode == MediaBranchMode::TranscodeFrame;
    planSharedNonStartupPolicy(plan);
    plan.audioServo.outputSampleRate = resolvedOutputAudioSampleRate;

    if (MediaRealtimeRequestClassifier::rawRtpInput(request)) {
        auto rtpInput = planRtpInput(request);
        if (!rtpInput) {
            return ::media::Result<MediaAvSyncPlan>::failure(rtpInput.error());
        }
        plan.sourceClockMode = MediaAvSyncSourceClockMode::RtpSenderReports;
        plan.controlGenerationPolicy =
            MediaControlGenerationPolicy::OptionalExactWhenPresent;
        plan.rtpInput = std::move(rtpInput).value();
        plan.startup.videoIdentity = plan.rtpInput->videoInput.identity;
        plan.startup.audioIdentity = plan.rtpInput->audioInput.identity;
        if (selectedTsProgram || preparedDemuxFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP input clock rejects MPEG-TS and demux input facts"));
        }
    } else if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) {
        if (!selectedTsProgram) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS A/V synchronization requires planner-selected program identity"));
        }
        if (preparedDemuxFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS input clock rejects demux timestamp facts"));
        }
        planTsInput(plan, request, *selectedTsProgram);
    } else if (MediaRealtimeRequestClassifier::realtimeUrlInput(request)) {
        if (selectedTsProgram || !preparedDemuxFacts ||
            preparedDemuxFacts->videoStreamIndex < 0 ||
            preparedDemuxFacts->audioStreamIndex < 0 ||
            preparedDemuxFacts->videoStreamIndex ==
                preparedDemuxFacts->audioStreamIndex ||
            !preparedDemuxFacts->videoTimeBase.isKnown() ||
            preparedDemuxFacts->videoTimeBase.num <= 0 ||
            preparedDemuxFacts->videoTimeBase.den <= 0 ||
            !preparedDemuxFacts->audioTimeBase.isKnown() ||
            preparedDemuxFacts->audioTimeBase.num <= 0 ||
            preparedDemuxFacts->audioTimeBase.den <= 0) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "URL A/V input clock requires prepared stream time bases"));
        }
        plan.sourceClockMode = MediaAvSyncSourceClockMode::DemuxTimestamps;
        plan.controlGenerationPolicy =
            MediaControlGenerationPolicy::RequiredExact;
        const std::string& groupIdentity = request.mediaId;
        plan.startup.videoIdentity = groupIdentity + ".stream." +
            std::to_string(preparedDemuxFacts->videoStreamIndex);
        plan.startup.audioIdentity = groupIdentity + ".stream." +
            std::to_string(preparedDemuxFacts->audioStreamIndex);
        plan.demuxTimestampInput.emplace(
            MediaAvSyncDemuxTimestampInputPlan{
                preparedDemuxFacts->videoTimeBase,
                preparedDemuxFacts->audioTimeBase,
                plan.startup.maximumInitialSkewNs,
                plan.recovery.hardDiscontinuityThresholdNs,
                1,
                MediaRunningTime::fromNanoseconds(0),
                preparedDemuxFacts->preparedInput,
                preparedDemuxFacts->preparedEvidence});
    } else {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Realtime A/V input clock is not supported"));
    }

    if (MediaRealtimeRequestClassifier::separateStreamsOutput(request)) {
        if (!MediaRealtimeRequestClassifier::rtpAvpOutput(request) ||
            resolvedTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Separate RTP output rejects MPEG-TS output facts"));
        }
        planRtpOutput(plan, request, resolvedOutputAudioSampleRate);
    } else if (MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        if (!resolvedTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output requires resolved H.264/AAC pipeline facts"));
        }
        auto outputMux = planTsOutput(plan, request, *resolvedTsFacts);
        if (!outputMux) {
            return ::media::Result<MediaAvSyncPlan>::failure(outputMux.error());
        }
        const bool useSharedNtpEpoch =
            outputMux.value().parameters().transportKind ==
            MediaOutputTransportKind::RtpAvp;
        plan.projectMpegTsOutput.emplace();
        plan.projectMpegTsOutput->outputMux = std::move(outputMux).value();
        plan.projectMpegTsOutput->useSharedNtpEpoch = useSharedNtpEpoch;
    } else {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Realtime A/V output adapter is not supported"));
    }

    if (auto status = MediaAvSyncPlanValidator::validatePolicy(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph

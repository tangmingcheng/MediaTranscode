#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/avsync/MediaAvOutputSynchronizationPlanner.h"

#include "internal/graph/planner/MediaRtpClockLivenessPolicy.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"

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

void planSourceNonStartupPolicy(MediaAvSyncPlan& plan)
{
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

    plan.recovery.suspectThresholdNs = runningTime(100 * Millisecond);
    plan.recovery.reacquisitionTimeoutNs = runningTime(10 * Second);
}

::media::Result<MediaAvSyncRtpInputPlan> planRtpInput(
    const MediaRealtimeRtpTranscodeRequest& request,
    MediaAvSourceLifecycleMode lifecycleMode)
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
    switch (lifecycleMode) {
    case MediaAvSourceLifecycleMode::FailSessionOnSourceLoss:
        input.input.clockLossPolicy = MediaRtpClockLossPolicy::FailOnDegraded;
        input.input.secondaryClockLossPolicy = MediaRtpClockLossPolicy::FailOnExpired;
        break;
    case MediaAvSourceLifecycleMode::PreserveActivatedOutput:
        input.input.clockLossPolicy = MediaRtpClockLossPolicy::InvalidateAndWait;
        input.input.secondaryClockLossPolicy = MediaRtpClockLossPolicy::InvalidateAndWait;
        break;
    default:
        return ::media::Result<MediaAvSyncRtpInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("Unknown source lifecycle mode"));
    }
    input.input.maximumInterStreamClockOffsetSkewNs =
        runningTime(50 * Millisecond);
    input.input.maximumSenderClockRateErrorPpm = 1'000;
    input.input.maximumSenderClockResidualNs = runningTime(250 * Millisecond);
    return ::media::Result<MediaAvSyncRtpInputPlan>::success(std::move(input));
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

} // namespace

::media::Result<MediaAvSyncRtpInputPlan> MediaAvSyncPlanner::planRtpInputClock(
    const MediaRealtimeRtpTranscodeRequest& request,
    MediaAvSourceLifecycleMode lifecycleMode)
{
    if (request.mediaId.empty()) {
        return ::media::Result<MediaAvSyncRtpInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V RTP input clock requires an explicit media identity"));
    }
    return planRtpInput(request, lifecycleMode);
}

::media::Result<MediaAvSyncPlan> MediaAvSyncPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsAudioVideoSelectedProgramPlan* selectedTsProgram,
    const MediaProjectMpegTsResolvedPipelineFacts* resolvedTsFacts,
    const MediaAvSyncPreparedDemuxTimestampFacts* preparedDemuxFacts,
    const MediaRealtimeGraphResourceLedgerPlan& resourceLedger,
    const MediaRealtimeDeploymentEnvelope& deployment,
    MediaBranchMode audioBranchMode,
    int resolvedOutputAudioSampleRate,
    MediaAvSourceLifecycleMode lifecycleMode)
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
    auto output = MediaAvOutputSynchronizationPlanner::plan(
        {request.mediaId, request.output.streamLayout, request.output.transport,
         request.parameters.video.frameRate, resolvedOutputAudioSampleRate,
         deployment, resolvedTsFacts});
    if (!output) return ::media::Result<MediaAvSyncPlan>::failure(output.error());
    auto plan = std::move(output).value();
    plan.domainRole = MediaAvSyncDomainRole::SharedSourceOutput;
    plan.sourceLifecycle = MediaAvSourceLifecyclePlan{lifecycleMode};
    if (preparedDemuxFacts) {
        auto finalized = MediaAvSyncStartupPolicyPlanner::finalizePrepared(
            preparedDemuxFacts->startup, resourceLedger, deployment);
        if (!finalized) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                finalized.error());
        }
        plan.startup = std::move(finalized).value();
    } else {
        auto startup = MediaAvSyncStartupPolicyPlanner::plan(
            request, resourceLedger, deployment);
        if (!startup) {
            return ::media::Result<MediaAvSyncPlan>::failure(startup.error());
        }
        plan.startup = std::move(startup).value();
    }
    plan.startup.trimAudioToCommonStart =
        audioBranchMode == MediaBranchMode::TranscodeFrame;
    planSourceNonStartupPolicy(plan);
    plan.audioServo.outputSampleRate = resolvedOutputAudioSampleRate;

    if (MediaRealtimeRequestClassifier::rawRtpInput(request)) {
        auto rtpInput = planRtpInput(request, lifecycleMode);
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

    if (auto status = MediaAvSyncPlanValidator::validatePolicy(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph

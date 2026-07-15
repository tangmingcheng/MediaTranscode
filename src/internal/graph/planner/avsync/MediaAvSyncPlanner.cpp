#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/sync/startup/MediaAvStartupLimits.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <cstdint>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t Millisecond = 1'000'000;
constexpr std::int64_t Second = 1'000'000'000;

constexpr MediaRunningTime runningTime(std::int64_t nanoseconds) noexcept
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

std::uint32_t stableIdentity(const std::string& value) noexcept
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : value) {
        hash = (hash ^ byte) * 16777619u;
    }
    return hash == 0 ? 1u : hash;
}

::media::Status planSharedPolicy(MediaAvSyncPlan& plan,
                                 const MediaRealtimeRtpTranscodeRequest& request)
{
    if (request.parameters.queues.packet == 0 ||
        request.parameters.queues.packet > MediaAvStartupMaximumUnitCapacity ||
        !request.avSyncStartup.maximumVideoUnitBytes ||
        !request.avSyncStartup.maximumAudioUnitBytes ||
        !request.avSyncStartup.maximumGap ||
        *request.avSyncStartup.maximumVideoUnitBytes == 0 ||
        *request.avSyncStartup.maximumAudioUnitBytes == 0 ||
        *request.avSyncStartup.maximumGap <= runningTime(0)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup requires explicit unit and byte capacity inputs"));
    }
    plan.masterClockMode = MediaAvSyncMasterClockMode::SteadyMonotonic;
    plan.canonicalTimeBaseNumerator = 1;
    plan.canonicalTimeBaseDenominator = 1'000'000'000;

    plan.startup.requireVideoKeyFrame = true;
    plan.startup.trimAudioToCommonStart = true;
    plan.startup.maximumWaitNs = runningTime(10 * Second);
    plan.startup.prerollNs = runningTime(500 * Millisecond);
    plan.startup.keyFrameWaitNs = runningTime(5 * Second);
    plan.startup.maximumAudioTrimNs = runningTime(250 * Millisecond);
    plan.startup.maximumInitialSkewNs = runningTime(40 * Millisecond);
    plan.startup.maximumGapNs = *request.avSyncStartup.maximumGap;
    plan.startup.outputLeadNs = runningTime(100 * Millisecond);
    plan.startup.videoCapacity = request.parameters.queues.packet;
    plan.startup.audioCapacity = request.parameters.queues.packet;
    const auto units = static_cast<std::uint64_t>(request.parameters.queues.packet);
    const auto videoUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumVideoUnitBytes);
    const auto audioUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumAudioUnitBytes);
    if (units > std::numeric_limits<std::uint64_t>::max() / videoUnitBytes ||
        units > std::numeric_limits<std::uint64_t>::max() / audioUnitBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup byte capacity is not representable"));
    }
    const auto videoByteCapacity = units * videoUnitBytes;
    const auto audioByteCapacity = units * audioUnitBytes;
    const auto maximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (videoUnitBytes > maximumSerialized || audioUnitBytes > maximumSerialized ||
        videoByteCapacity > maximumSerialized || audioByteCapacity > maximumSerialized) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup capacity exceeds the runtime option range"));
    }
    plan.startup.videoByteCapacity = videoByteCapacity;
    plan.startup.audioByteCapacity = audioByteCapacity;
    plan.startup.maximumVideoUnitBytes = videoUnitBytes;
    plan.startup.maximumAudioUnitBytes = audioUnitBytes;
    plan.startup.allowDegradedClock = false;

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
    return ::media::Status::success();
}

::media::Result<MediaAvSyncPlan> planRtp(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.videoRtp.payloadType || !request.input.videoRtp.clockRate ||
        !request.input.audioRtp.payloadType || !request.input.audioRtp.clockRate) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized separate RTP requires explicit audio/video payload types and clock rates"));
    }

    MediaAvSyncPlan plan;
    if (auto status = planSharedPolicy(plan, request); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    plan.topology = MediaAvSyncTopology::SeparateRtpToSeparateRtp;
    plan.sourceClockMode = MediaAvSyncSourceClockMode::RtpSenderReports;
    plan.rtp.emplace();
    const std::string groupIdentity = request.mediaId.empty()
        ? std::string("realtime-av-sync")
        : request.mediaId;
    const std::string cname =
        "mt-" + std::to_string(stableIdentity(groupIdentity)) + "@media-transcode";

    plan.rtp->videoInput.identity = groupIdentity + ".input.video";
    plan.rtp->videoInput.payloadType = *request.input.videoRtp.payloadType;
    plan.rtp->videoInput.clockRate = *request.input.videoRtp.clockRate;
    plan.rtp->audioInput.identity = groupIdentity + ".input.audio";
    plan.startup.videoIdentity = *plan.rtp->videoInput.identity;
    plan.startup.audioIdentity = *plan.rtp->audioInput.identity;
    plan.rtp->audioInput.payloadType = *request.input.audioRtp.payloadType;
    plan.rtp->audioInput.clockRate = *request.input.audioRtp.clockRate;
    plan.rtp->input.requireCommonCname = true;
    plan.rtp->input.requireSenderReports = true;
    plan.rtp->input.senderReportTimeoutNs = runningTime(3 * Second);
    plan.rtp->input.maximumExtrapolationNs = runningTime(5 * Second);
    plan.rtp->input.maximumSenderReportSkewNs = runningTime(50 * Millisecond);
    plan.rtp->input.maximumSenderClockRateErrorPpm = 1'000;
    plan.rtp->input.maximumSenderClockResidualNs = runningTime(250 * Millisecond);

    const int audioOutputRate = request.parameters.audio.sampleRate.value_or(
        *request.input.audioRtp.clockRate);
    plan.audioServo.outputSampleRate = audioOutputRate;
    plan.rtp->videoOutput.identity = groupIdentity + ".output.video";
    plan.rtp->videoOutput.payloadType = 96;
    plan.rtp->videoOutput.clockRate = 90000;
    plan.rtp->videoOutput.ssrc = stableIdentity(*plan.rtp->videoOutput.identity);
    plan.rtp->videoOutput.baseTimestamp = stableIdentity(groupIdentity + ".video.timestamp");
    plan.rtp->videoOutput.cname = cname;
    plan.rtp->audioOutput.identity = groupIdentity + ".output.audio";
    plan.rtp->audioOutput.payloadType = 97;
    plan.rtp->audioOutput.clockRate = audioOutputRate;
    plan.rtp->audioOutput.ssrc = stableIdentity(*plan.rtp->audioOutput.identity);
    plan.rtp->audioOutput.baseTimestamp = stableIdentity(groupIdentity + ".audio.timestamp");
    plan.rtp->audioOutput.cname = cname;
    plan.rtp->output.useSharedNtpEpoch = true;
    plan.rtp->output.senderReportIntervalNs = runningTime(Second);

    if (auto status = MediaAvSyncPlanValidator::validatePolicy(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

::media::Result<MediaAvSyncPlan> planTs(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsSelectedProgramPlan& selected,
    const MediaProjectMpegTsResolvedPipelineFacts& resolvedFacts)
{
    MediaAvSyncPlan plan;
    if (auto status = planSharedPolicy(plan, request); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    plan.topology = MediaAvSyncTopology::MpegTsToMpegTs;
    plan.sourceClockMode = MediaAvSyncSourceClockMode::MpegTsPcr;
    plan.ts.emplace();
    plan.ts->programNumber = selected.programNumber;
    plan.ts->programMapPid = selected.programMapPid;
    plan.ts->videoPid = selected.videoPid;
    plan.ts->audioPid = selected.audioPid;
    const std::string groupIdentity = request.mediaId.empty()
        ? std::string("realtime-av-sync-ts")
        : request.mediaId;
    plan.startup.videoIdentity = groupIdentity + ".pid." +
                                 std::to_string(selected.videoPid);
    plan.startup.audioIdentity = groupIdentity + ".pid." +
                                 std::to_string(selected.audioPid);
    plan.ts->pcrPid = selected.pcrPid;

    if (!plan.startup.outputLeadNs) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires planner-owned startup output lead"));
    }
    auto resolvedOutput = MediaProjectMpegTsOutputPlan::create(
        resolvedFacts.videoCodecName, resolvedFacts.audioOutput,
        *plan.startup.outputLeadNs);
    if (!resolvedOutput) {
        return ::media::Result<MediaAvSyncPlan>::failure(resolvedOutput.error());
    }
    plan.audioServo.outputSampleRate = resolvedOutput.value().audioSampleRate();
    plan.ts->outputMux = resolvedOutput.value().muxPlan();

    if (auto status = MediaAvSyncPlanValidator::validatePolicy(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

} // namespace

::media::Result<MediaAvSyncPlan> MediaAvSyncPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsSelectedProgramPlan* selectedTsProgram,
    const MediaProjectMpegTsResolvedPipelineFacts* resolvedTsFacts)
{
    if (!request.parameters.execution.includeAudio) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported("A/V synchronization requires both audio and video"));
    }
    if (MediaRealtimeRequestClassifier::rawRtpInput(request) &&
        MediaRealtimeRequestClassifier::separateRtpOutput(request)) {
        if (resolvedTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP A/V synchronization does not accept an MPEG-TS resolved output plan"));
        }
        return planRtp(request);
    }
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request) &&
        MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        if (!selectedTsProgram) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS A/V synchronization requires planner-selected program identity"));
        }
        if (!resolvedTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS A/V synchronization requires resolved output media facts"));
        }
        return planTs(request, *selectedTsProgram, *resolvedTsFacts);
    }
    if (MediaRealtimeRequestClassifier::rawRtpInput(request) &&
        MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized separate RTP input to MPEG-TS output is not supported"));
    }
    return ::media::Result<MediaAvSyncPlan>::failure(
        ::media::ErrorInfo::unsupported("Realtime A/V synchronization topology is not supported"));
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
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

std::uint32_t stableIdentity(const std::string& value) noexcept
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : value) {
        hash = (hash ^ byte) * 16777619u;
    }
    return hash == 0 ? 1u : hash;
}

void planSharedPolicy(MediaAvSyncPlan& plan)
{
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
    plan.startup.outputLeadNs = runningTime(100 * Millisecond);

    plan.audioServo.deadbandNs = runningTime(Millisecond);
    plan.audioServo.shortControlWindowNs = runningTime(250 * Millisecond);
    plan.audioServo.longControlWindowNs = runningTime(5 * Second);
    plan.audioServo.proportionalGainPpm = 250;
    plan.audioServo.integralGainPpm = 50;
    plan.audioServo.maximumSlewPpmPerSecond = 100;
    plan.audioServo.normalCorrectionLimitPpm = 1000;
    plan.audioServo.recoveryCorrectionLimitPpm = 5000;
    plan.audioServo.compensationWindowNs = runningTime(Second);

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

::media::Result<MediaAvSyncPlan> planRtp(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.videoRtp.payloadType || !request.input.videoRtp.clockRate ||
        !request.input.audioRtp.payloadType || !request.input.audioRtp.clockRate) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized separate RTP requires explicit audio/video payload types and clock rates"));
    }

    MediaAvSyncPlan plan;
    planSharedPolicy(plan);
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
    plan.rtp->audioInput.payloadType = *request.input.audioRtp.payloadType;
    plan.rtp->audioInput.clockRate = *request.input.audioRtp.clockRate;
    plan.rtp->input.requireCommonCname = true;
    plan.rtp->input.requireSenderReports = true;
    plan.rtp->input.senderReportTimeoutNs = runningTime(3 * Second);
    plan.rtp->input.maximumExtrapolationNs = runningTime(5 * Second);

    const int audioOutputRate = request.parameters.audio.sampleRate.value_or(
        *request.input.audioRtp.clockRate);
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

    if (auto status = MediaAvSyncPlanValidator::validate(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

::media::Result<MediaAvSyncPlan> planTs()
{
    MediaAvSyncPlan plan;
    planSharedPolicy(plan);
    plan.topology = MediaAvSyncTopology::MpegTsToMpegTs;
    plan.sourceClockMode = MediaAvSyncSourceClockMode::MpegTsPcr;
    plan.ts.emplace();
    plan.ts->programNumber = 1;
    plan.ts->programMapPid = 4096;
    plan.ts->videoPid = 256;
    plan.ts->audioPid = 257;
    plan.ts->pcrPid = 256;
    plan.ts->pcrIntervalNs = runningTime(20 * Millisecond);
    plan.ts->maximumPcrGapNs = runningTime(100 * Millisecond);
    plan.ts->maximumPcrJitterNs = runningTime(5 * Millisecond);
    plan.ts->timestampTimeBaseNumerator = 1;
    plan.ts->timestampTimeBaseDenominator = 90000;

    if (auto status = MediaAvSyncPlanValidator::validate(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

} // namespace

::media::Result<MediaAvSyncPlan> MediaAvSyncPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.parameters.execution.includeAudio) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported("A/V synchronization requires both audio and video"));
    }
    if (MediaRealtimeRequestClassifier::rawRtpInput(request) &&
        MediaRealtimeRequestClassifier::separateRtpOutput(request)) {
        return planRtp(request);
    }
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request) &&
        MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        return planTs();
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

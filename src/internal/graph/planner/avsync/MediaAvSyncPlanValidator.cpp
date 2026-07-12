#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* field)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            std::string("Incomplete or inconsistent A/V synchronization plan: ") + field));
}

template <typename T>
bool positive(const std::optional<T>& value)
{
    return value && *value > 0;
}

bool positive(const std::optional<MediaRunningTime>& value)
{
    return value && *value > MediaRunningTime::fromNanoseconds(0);
}

bool presentText(const std::optional<std::string>& value)
{
    return value && !value->empty();
}

::media::Status validateShared(const MediaAvSyncPlan& plan)
{
    if (!plan.topology) return invalid("topology");
    if (!plan.sourceClockMode) return invalid("sourceClockMode");
    if (!plan.masterClockMode ||
        *plan.masterClockMode != MediaAvSyncMasterClockMode::SteadyMonotonic) {
        return invalid("masterClockMode");
    }
    if (!positive(plan.canonicalTimeBaseNumerator) ||
        !positive(plan.canonicalTimeBaseDenominator)) {
        return invalid("canonicalTimeBase");
    }

    const auto& startup = plan.startup;
    if (!startup.requireVideoKeyFrame || !*startup.requireVideoKeyFrame ||
        !startup.trimAudioToCommonStart || !*startup.trimAudioToCommonStart ||
        !positive(startup.maximumWaitNs) || !positive(startup.prerollNs) ||
        !positive(startup.keyFrameWaitNs) || !positive(startup.maximumAudioTrimNs) ||
        !positive(startup.maximumInitialSkewNs) || !positive(startup.outputLeadNs) ||
        *startup.maximumAudioTrimNs > *startup.prerollNs ||
        *startup.maximumInitialSkewNs >= *startup.outputLeadNs ||
        *startup.prerollNs >= *startup.keyFrameWaitNs ||
        *startup.keyFrameWaitNs > *startup.maximumWaitNs) {
        return invalid("ordered startup thresholds");
    }

    const auto& servo = plan.audioServo;
    if (!positive(servo.deadbandNs) || !positive(servo.shortControlWindowNs) ||
        !positive(servo.longControlWindowNs) ||
        !positive(servo.proportionalGainPpm) || !positive(servo.integralGainPpm) ||
        !positive(servo.maximumSlewPpmPerSecond) ||
        !positive(servo.normalCorrectionLimitPpm) ||
        !positive(servo.recoveryCorrectionLimitPpm) ||
        !positive(servo.compensationWindowNs) ||
        *servo.deadbandNs >= *servo.shortControlWindowNs ||
        *servo.shortControlWindowNs >= *servo.compensationWindowNs ||
        *servo.compensationWindowNs >= *servo.longControlWindowNs ||
        *servo.normalCorrectionLimitPpm > 1000 ||
        *servo.recoveryCorrectionLimitPpm > 5000 ||
        *servo.maximumSlewPpmPerSecond > *servo.normalCorrectionLimitPpm ||
        *servo.normalCorrectionLimitPpm > *servo.recoveryCorrectionLimitPpm) {
        return invalid("audio servo policy");
    }

    const auto& video = plan.video;
    if (!positive(video.earlyHoldThresholdNs) ||
        !positive(video.lateDisplayThresholdNs) ||
        !positive(video.dropThresholdNs) || !video.allowRecoveryRepeat ||
        !positive(video.maximumConsecutiveRecoveryActions) ||
        *video.earlyHoldThresholdNs >= *video.lateDisplayThresholdNs ||
        *video.lateDisplayThresholdNs >= *video.dropThresholdNs) {
        return invalid("ordered video thresholds");
    }

    const auto& recovery = plan.recovery;
    if (!positive(recovery.suspectThresholdNs) ||
        !positive(recovery.hardDiscontinuityThresholdNs) ||
        !positive(recovery.reacquisitionTimeoutNs) ||
        *video.dropThresholdNs >= *recovery.suspectThresholdNs ||
        *recovery.suspectThresholdNs >= *recovery.hardDiscontinuityThresholdNs ||
        *recovery.hardDiscontinuityThresholdNs >= *recovery.reacquisitionTimeoutNs) {
        return invalid("ordered recovery thresholds");
    }

    const auto& metrics = plan.metrics;
    if (!metrics.collectStateAndGeneration || !*metrics.collectStateAndGeneration ||
        !metrics.collectClockEvidence || !*metrics.collectClockEvidence ||
        !metrics.collectQueueDurations || !*metrics.collectQueueDurations ||
        !metrics.collectPhaseErrors || !*metrics.collectPhaseErrors ||
        !metrics.collectAudioCorrection || !*metrics.collectAudioCorrection ||
        !metrics.collectVideoRecoveryCounts || !*metrics.collectVideoRecoveryCounts ||
        !metrics.collectDiscontinuityCounts || !*metrics.collectDiscontinuityCounts ||
        !metrics.collectProtocolClockHealth || !*metrics.collectProtocolClockHealth ||
        !positive(metrics.maximumStartupSkewNs) ||
        !positive(metrics.maximumSteadyP95SkewNs) ||
        !positive(metrics.maximumSteadyP99SkewNs) ||
        !positive(metrics.maximumDriftNsPerHour) ||
        *metrics.maximumSteadyP95SkewNs > *metrics.maximumSteadyP99SkewNs ||
        *metrics.maximumSteadyP99SkewNs > *metrics.maximumStartupSkewNs) {
        return invalid("metrics and acceptance thresholds");
    }
    return ::media::Status::success();
}

bool validRtpInputStream(const MediaAvSyncRtpInputStreamPlan& stream)
{
    return presentText(stream.identity) && stream.payloadType &&
           *stream.payloadType >= 0 && *stream.payloadType <= 127 &&
           positive(stream.clockRate);
}

bool validRtpOutputStream(const MediaAvSyncRtpOutputStreamPlan& stream)
{
    return presentText(stream.identity) && stream.payloadType &&
           *stream.payloadType >= 0 && *stream.payloadType <= 127 &&
           positive(stream.clockRate) && positive(stream.ssrc) &&
           stream.baseTimestamp && presentText(stream.cname);
}

::media::Status validateRtp(const MediaAvSyncPlan& plan)
{
    if (plan.ts || !plan.rtp ||
        *plan.sourceClockMode != MediaAvSyncSourceClockMode::RtpSenderReports) {
        return invalid("RTP topology clock contract");
    }
    const auto& rtp = *plan.rtp;
    if (!validRtpInputStream(rtp.videoInput) || !validRtpInputStream(rtp.audioInput) ||
        !rtp.input.requireCommonCname || !*rtp.input.requireCommonCname ||
        !rtp.input.requireSenderReports || !*rtp.input.requireSenderReports ||
        !positive(rtp.input.senderReportTimeoutNs) ||
        !positive(rtp.input.maximumExtrapolationNs) ||
        !positive(rtp.input.maximumSenderReportSkewNs) ||
        !rtp.input.maximumSenderClockRateErrorPpm ||
        *rtp.input.maximumSenderClockRateErrorPpm <= 0 ||
        *rtp.input.maximumSenderClockRateErrorPpm >= 1'000'000 ||
        !positive(rtp.input.maximumSenderClockResidualNs) ||
        *rtp.input.senderReportTimeoutNs >= *rtp.input.maximumExtrapolationNs ||
        *rtp.input.maximumExtrapolationNs >= *plan.recovery.reacquisitionTimeoutNs ||
        *rtp.input.maximumSenderReportSkewNs >= *plan.recovery.hardDiscontinuityThresholdNs) {
        return invalid("RTP input identities, CNAME, or sender report policy");
    }
    if (!validRtpOutputStream(rtp.videoOutput) ||
        !validRtpOutputStream(rtp.audioOutput) ||
        *rtp.videoOutput.ssrc == *rtp.audioOutput.ssrc ||
        *rtp.videoOutput.cname != *rtp.audioOutput.cname ||
        !rtp.output.useSharedNtpEpoch || !*rtp.output.useSharedNtpEpoch ||
        !positive(rtp.output.senderReportIntervalNs) ||
        *rtp.output.senderReportIntervalNs >= *rtp.input.senderReportTimeoutNs) {
        return invalid("RTP output identities, CNAME, or sender report policy");
    }
    return ::media::Status::success();
}

::media::Status validateTs(const MediaAvSyncPlan& plan)
{
    if (plan.rtp || !plan.ts ||
        *plan.sourceClockMode != MediaAvSyncSourceClockMode::MpegTsPcr) {
        return invalid("MPEG-TS topology clock contract");
    }
    const auto& ts = *plan.ts;
    constexpr int MinimumAssignablePid = 0x0020;
    constexpr int NullPid = 0x1FFF;
    if (!positive(ts.programNumber) || *ts.programNumber > 0xFFFF ||
        !ts.programMapPid || *ts.programMapPid < MinimumAssignablePid || *ts.programMapPid >= NullPid ||
        !ts.videoPid || *ts.videoPid < MinimumAssignablePid || *ts.videoPid >= NullPid ||
        !ts.audioPid || *ts.audioPid < MinimumAssignablePid || *ts.audioPid >= NullPid ||
        !ts.pcrPid || *ts.pcrPid < MinimumAssignablePid || *ts.pcrPid >= NullPid ||
        *ts.programMapPid == *ts.videoPid || *ts.programMapPid == *ts.audioPid ||
        *ts.programMapPid == *ts.pcrPid ||
        *ts.videoPid == *ts.audioPid ||
        (*ts.pcrPid != *ts.videoPid && *ts.pcrPid != *ts.audioPid) ||
        !positive(ts.pcrIntervalNs) || !positive(ts.maximumPcrGapNs) ||
        !positive(ts.maximumPcrJitterNs) ||
        *ts.maximumPcrJitterNs >= *ts.pcrIntervalNs ||
        *ts.pcrIntervalNs >= *ts.maximumPcrGapNs ||
        !positive(ts.timestampTimeBaseNumerator) ||
        !positive(ts.timestampTimeBaseDenominator)) {
        return invalid("MPEG-TS program, PID, PCR, or timestamp policy");
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaAvSyncPlanValidator::validate(const MediaAvSyncPlan& plan)
{
    if (auto status = validateShared(plan); !status) return status;
    switch (*plan.topology) {
    case MediaAvSyncTopology::SeparateRtpToSeparateRtp:
        return validateRtp(plan);
    case MediaAvSyncTopology::MpegTsToMpegTs:
        return validateTs(plan);
    }
    return invalid("topology");
}

} // namespace media::ffmpeg::graph

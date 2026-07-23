#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"
#include "internal/graph/sync/MediaAudioDriftServoPolicyValidator.h"
#include "internal/graph/sync/startup/MediaAvStartupLimits.h"

#include <optional>
#include <limits>
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

bool validByteCapacity(const std::optional<std::size_t>& units,
                       const std::optional<std::uint64_t>& maximumUnitBytes,
                       const std::optional<std::uint64_t>& bytes)
{
    constexpr auto MaximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    return positive(units) && positive(maximumUnitBytes) && positive(bytes) &&
           *maximumUnitBytes <= MaximumSerialized && *bytes <= MaximumSerialized &&
           *units <= std::numeric_limits<std::uint64_t>::max() / *maximumUnitBytes &&
           *bytes == static_cast<std::uint64_t>(*units) * *maximumUnitBytes;
}

::media::Status validateShared(const MediaAvSyncPlan& plan, bool finalized)
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
        !positive(startup.maximumGapNs) ||
        *startup.maximumAudioTrimNs > *startup.prerollNs ||
        *startup.maximumGapNs >= *startup.prerollNs ||
        *startup.maximumInitialSkewNs >= *startup.outputLeadNs ||
        *startup.prerollNs >= *startup.keyFrameWaitNs ||
        *startup.keyFrameWaitNs > *startup.maximumWaitNs ||
        !positive(startup.videoCapacity) || !positive(startup.audioCapacity) ||
        !validByteCapacity(startup.videoCapacity, startup.maximumVideoUnitBytes,
                           startup.videoByteCapacity) ||
        !validByteCapacity(startup.audioCapacity, startup.maximumAudioUnitBytes,
                           startup.audioByteCapacity) ||
        *startup.videoCapacity > MediaAvStartupMaximumUnitCapacity ||
        *startup.audioCapacity > MediaAvStartupMaximumUnitCapacity ||
        !presentText(startup.videoIdentity) || !presentText(startup.audioIdentity) ||
        *startup.videoIdentity == *startup.audioIdentity ||
        !startup.allowDegradedClock || *startup.allowDegradedClock) {
        return invalid("ordered startup thresholds");
    }

    const auto& servo = plan.audioServo;
    if (!positive(servo.deadbandNs) ||
        !positive(servo.phaseFilterTimeConstantNs) ||
        !positive(servo.proportionalGainPpmPerSecond) ||
        !positive(servo.integralGainPpmPerSecondSquared) ||
        !positive(servo.integratorLimitPpm) ||
        !servo.frequencyFeedForwardNumerator ||
        *servo.frequencyFeedForwardNumerator < 0 ||
        !positive(servo.frequencyFeedForwardDenominator) ||
        *servo.frequencyFeedForwardNumerator >
            *servo.frequencyFeedForwardDenominator ||
        !servo.frequencyDeadbandPpm || *servo.frequencyDeadbandPpm < 0 ||
        !positive(servo.maximumMeasuredFrequencyPpm) ||
        !positive(servo.recoveryExitFrequencyPpm) ||
        *servo.frequencyDeadbandPpm >= *servo.recoveryExitFrequencyPpm ||
        *servo.recoveryExitFrequencyPpm >= *servo.maximumMeasuredFrequencyPpm ||
        !servo.antiWindupMode ||
        *servo.antiWindupMode !=
            MediaAudioServoAntiWindupMode::ConditionalIntegration ||
        !positive(servo.minimumUpdateIntervalNs) ||
        !positive(servo.maximumMeasurementGapNs) ||
        !positive(servo.maximumSlewPpmPerSecond) ||
        !positive(servo.normalCorrectionLimitPpm) ||
        !positive(servo.recoveryCorrectionLimitPpm) ||
        !positive(servo.recoveryEnterThresholdNs) ||
        !positive(servo.recoveryExitThresholdNs) ||
        !positive(servo.recoveryExitHoldNs) ||
        !positive(servo.outputSampleRate) ||
        !positive(servo.correctionLookaheadWindows) ||
        *servo.minimumUpdateIntervalNs >= *servo.phaseFilterTimeConstantNs ||
        *servo.phaseFilterTimeConstantNs >= *servo.maximumMeasurementGapNs ||
        *servo.deadbandNs >= *servo.recoveryExitThresholdNs ||
        *servo.recoveryExitThresholdNs >= *servo.recoveryEnterThresholdNs ||
        !positive(plan.recovery.hardDiscontinuityThresholdNs) ||
        *servo.recoveryEnterThresholdNs >=
            *plan.recovery.hardDiscontinuityThresholdNs ||
        *servo.recoveryExitHoldNs < *servo.minimumUpdateIntervalNs ||
        *servo.correctionLookaheadWindows >
            MediaAudioDriftServoLimits::MaximumCorrectionLookaheadWindows ||
        *servo.outputSampleRate > MediaAudioDriftServoLimits::MaximumOutputSampleRate ||
        servo.maximumMeasurementGapNs->nanoseconds() >
            MediaAudioDriftServoLimits::MaximumMeasurementGapNs ||
        servo.recoveryExitHoldNs->nanoseconds() >
            MediaAudioDriftServoLimits::MaximumPolicyDurationNs ||
        *servo.proportionalGainPpmPerSecond >
            MediaAudioDriftServoLimits::MaximumProportionalGainPpmPerSecond ||
        *servo.integralGainPpmPerSecondSquared >
            MediaAudioDriftServoLimits::MaximumIntegralGainPpmPerSecondSquared ||
        *servo.maximumMeasuredFrequencyPpm >
            MediaAudioDriftServoLimits::MaximumMeasuredFrequencyPpm ||
        plan.recovery.hardDiscontinuityThresholdNs->nanoseconds() >
            MediaAudioDriftServoLimits::MaximumHardDiscontinuityNs ||
        *servo.normalCorrectionLimitPpm >
            MediaAudioDriftServoLimits::MaximumNormalCorrectionPpm ||
        *servo.recoveryCorrectionLimitPpm >
            MediaAudioDriftServoLimits::MaximumRecoveryCorrectionPpm ||
        *servo.integratorLimitPpm > *servo.recoveryCorrectionLimitPpm ||
        *servo.maximumSlewPpmPerSecond > *servo.normalCorrectionLimitPpm ||
        *servo.normalCorrectionLimitPpm > *servo.recoveryCorrectionLimitPpm) {
        return invalid("audio servo policy");
    }
    if (!finalized) {
        if (servo.commandLeadNs || servo.compensationWindowNs ||
            servo.frequencyFilterTimeConstantNs) {
            return invalid("incomplete policy contains finalized correction timing");
        }
    } else if (
        !positive(servo.frequencyFilterTimeConstantNs) ||
        !positive(servo.compensationWindowNs) || !positive(servo.commandLeadNs) ||
        !MediaAudioDriftServoPolicyValidator::leadCoversWorstPositiveGap(
            servo.commandLeadNs->nanoseconds(),
            servo.maximumMeasurementGapNs->nanoseconds(),
            *servo.outputSampleRate, *servo.recoveryCorrectionLimitPpm) ||
        *servo.maximumMeasurementGapNs >= *servo.commandLeadNs ||
        *servo.commandLeadNs >= *servo.compensationWindowNs ||
        *servo.compensationWindowNs >= *servo.frequencyFilterTimeConstantNs ||
        servo.compensationWindowNs->nanoseconds() >
            (std::numeric_limits<std::int64_t>::max() -
             MediaAudioDriftServoLimits::NanosecondsPerSecond) /
                *servo.outputSampleRate ||
        servo.compensationWindowNs->nanoseconds() *
                static_cast<std::int64_t>(*servo.outputSampleRate) <
            MediaAudioDriftServoLimits::NanosecondsPerSecond ||
        servo.compensationWindowNs->nanoseconds() *
                static_cast<std::int64_t>(*servo.outputSampleRate) /
                MediaAudioDriftServoLimits::NanosecondsPerSecond >
            std::numeric_limits<int>::max() ||
        servo.frequencyFilterTimeConstantNs->nanoseconds() >
            MediaAudioDriftServoLimits::MaximumPolicyDurationNs ||
        servo.compensationWindowNs->nanoseconds() >
            MediaAudioDriftServoLimits::MaximumPolicyDurationNs) {
        return invalid("finalized audio correction timing");
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
        rtp.input.commonEpochPolicy !=
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime ||
        rtp.input.streamAssociationMode !=
            MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair ||
        !rtp.input.rtcpCompositionMode ||
        *rtp.input.rtcpCompositionMode !=
            MediaRtcpCompositionMode::ReducedSizeRfc5506 ||
        !positive(rtp.input.identityEvidenceTimeoutNs) ||
        !rtp.input.requireSenderReports || !*rtp.input.requireSenderReports ||
        !positive(rtp.input.senderReportTimeoutNs) ||
        !positive(rtp.input.maximumExtrapolationNs) ||
        *rtp.input.identityEvidenceTimeoutNs !=
            *rtp.input.maximumExtrapolationNs ||
        !positive(rtp.input.maximumInterStreamClockOffsetSkewNs) ||
        !rtp.input.maximumSenderClockRateErrorPpm ||
        *rtp.input.maximumSenderClockRateErrorPpm <= 0 ||
        *rtp.input.maximumSenderClockRateErrorPpm >= 1'000'000 ||
        !positive(rtp.input.maximumSenderClockResidualNs) ||
        *rtp.input.senderReportTimeoutNs >= *rtp.input.maximumExtrapolationNs ||
        *rtp.input.maximumExtrapolationNs >= *plan.recovery.reacquisitionTimeoutNs ||
        *rtp.input.maximumInterStreamClockOffsetSkewNs >=
            *plan.recovery.hardDiscontinuityThresholdNs) {
        return invalid("RTP input association, RTCP, or sender report policy");
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
        !ts.outputMux) {
        return invalid("MPEG-TS input selection or output mux plan");
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaAvSyncPlanValidator::validate(const MediaAvSyncPlan& plan)
{
    if (auto status = validateShared(plan, true); !status) return status;
    switch (*plan.topology) {
    case MediaAvSyncTopology::SeparateRtpToSeparateRtp:
        return validateRtp(plan);
    case MediaAvSyncTopology::MpegTsToMpegTs:
        return validateTs(plan);
    }
    return invalid("topology");
}

::media::Status MediaAvSyncPlanValidator::validatePolicy(
    const MediaAvSyncPlan& plan)
{
    if (auto status = validateShared(plan, false); !status) return status;
    switch (*plan.topology) {
    case MediaAvSyncTopology::SeparateRtpToSeparateRtp:
        return validateRtp(plan);
    case MediaAvSyncTopology::MpegTsToMpegTs:
        return validateTs(plan);
    }
    return invalid("topology");
}

} // namespace media::ffmpeg::graph

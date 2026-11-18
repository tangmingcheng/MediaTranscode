#pragma once
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include <tuple>

namespace media::ffmpeg::graph {
// Numeric snapshot. Recreate after changing candidate quantities. Identities
// borrow an lvalue owner that must outlive every use of this view.
struct MediaAvSyncStartupPolicyView final : MediaAvSyncStartupCapacity {
    std::optional<MediaVideoStartupReadiness> videoReadiness;
    std::optional<bool> requireVideoKeyFrame;
    std::optional<bool> trimAudioToCommonStart;
    std::optional<MediaRunningTime> maximumWaitNs;
    std::optional<MediaRunningTime> prerollNs;
    std::optional<MediaRunningTime> keyFrameWaitNs;
    std::optional<MediaRunningTime> maximumAudioTrimNs;
    std::optional<MediaRunningTime> maximumInitialSkewNs;
    std::optional<MediaRunningTime> outputLeadNs;
    const std::optional<std::string>& videoIdentity;
    const std::optional<std::string>& audioIdentity;
    std::optional<bool> allowDegradedClock;
    MediaAvSyncStartupPolicyView(const MediaAvSyncStartupPolicy& policy)
        : MediaAvSyncStartupPolicyView(policy, static_cast<const MediaAvSyncStartupCapacity&>(policy)) {}
    MediaAvSyncStartupPolicyView(const MediaAvSyncStartupFixedPolicy& policy, const MediaAvSyncStartupCapacity& capacity)
        : MediaAvSyncStartupCapacity(capacity),
          videoReadiness(policy.videoReadiness), requireVideoKeyFrame(policy.requireVideoKeyFrame),
            trimAudioToCommonStart(policy.trimAudioToCommonStart), maximumWaitNs(policy.maximumWaitNs),
            prerollNs(policy.prerollNs), keyFrameWaitNs(policy.keyFrameWaitNs),
            maximumAudioTrimNs(policy.maximumAudioTrimNs), maximumInitialSkewNs(policy.maximumInitialSkewNs),
            outputLeadNs(policy.outputLeadNs),
            videoIdentity(policy.videoIdentity), audioIdentity(policy.audioIdentity),
            allowDegradedClock(policy.allowDegradedClock) {}
    MediaAvSyncStartupPolicyView(MediaAvSyncStartupFixedPolicy&&, const MediaAvSyncStartupCapacity&) = delete;
    MediaAvSyncStartupPolicyView(const MediaAvSyncStartupFixedPolicy&&, const MediaAvSyncStartupCapacity&) = delete;
    MediaAvSyncStartupPolicyView(MediaAvSyncStartupPolicy&&) = delete;
    MediaAvSyncStartupPolicyView(const MediaAvSyncStartupPolicy&&) = delete;
    MediaAvSyncStartupPolicyView(MediaAvSyncStartupPolicy&&, const MediaAvSyncStartupCapacity&) = delete;
    MediaAvSyncStartupPolicyView(const MediaAvSyncStartupPolicy&&, const MediaAvSyncStartupCapacity&) = delete;
    bool operator==(const MediaAvSyncStartupPolicyView& other) const {
        return std::tie(videoReadiness, requireVideoKeyFrame, trimAudioToCommonStart, maximumWaitNs, prerollNs,
            keyFrameWaitNs, maximumAudioTrimNs, maximumInitialSkewNs, maximumGapNs, outputLeadNs, videoCapacity,
            audioCapacity, videoByteCapacity, audioByteCapacity, maximumVideoUnitBytes, maximumAudioUnitBytes,
            videoIdentity, audioIdentity, allowDegradedClock) ==
            std::tie(other.videoReadiness, other.requireVideoKeyFrame, other.trimAudioToCommonStart,
            other.maximumWaitNs, other.prerollNs, other.keyFrameWaitNs, other.maximumAudioTrimNs,
            other.maximumInitialSkewNs, other.maximumGapNs, other.outputLeadNs, other.videoCapacity,
            other.audioCapacity, other.videoByteCapacity, other.audioByteCapacity, other.maximumVideoUnitBytes,
            other.maximumAudioUnitBytes, other.videoIdentity, other.audioIdentity, other.allowDegradedClock);
    }
};

// Fixed products are borrowed; startup and servo are allocation-free numeric
// snapshots. Construct a fresh view for each synchronous candidate evaluation.
struct MediaAvSyncPlanView final {
    const std::optional<MediaTranscodeStreamSet>& members;
    const std::optional<MediaAvSyncDomainRole>& domainRole;
    const std::optional<MediaAvSourceLifecyclePlan>& sourceLifecycle;
    const std::optional<MediaAvSyncSourceClockMode>& sourceClockMode;
    const std::optional<MediaControlGenerationPolicy>& controlGenerationPolicy;
    const std::optional<MediaAvSyncMasterClockMode>& masterClockMode;
    const std::optional<int>& canonicalTimeBaseNumerator;
    const std::optional<int>& canonicalTimeBaseDenominator;
    MediaAvSyncStartupPolicyView startup;
    MediaAvSyncAudioServoPolicy audioServo;
    const MediaAvSyncVideoPolicy& video;
    const MediaAvSyncRecoveryPolicy& recovery;
    const std::optional<MediaAvSyncRtpInputPlan>& rtpInput;
    const std::optional<MediaAvSyncMpegTsInputPlan>& mpegTsInput;
    const std::optional<MediaAvSyncDemuxTimestampInputPlan>& demuxTimestampInput;
    const std::optional<MediaAvSyncRtpOutputPlan>& rtpOutput;
    const std::optional<MediaAvSyncProjectMpegTsOutputPlan>& projectMpegTsOutput;
    const MediaAvSyncMetricsPolicy& metrics;
    MediaAvSyncPlanView(const MediaAvSyncPlan& plan)
        : MediaAvSyncPlanView(plan, MediaAvSyncStartupPolicyView(plan.startup), plan.audioServo) {}
    MediaAvSyncPlanView(const MediaAvSyncFixedPlan& plan, const MediaAvSyncStartupCapacity& capacity)
        : MediaAvSyncPlanView(plan, MediaAvSyncStartupPolicyView(plan.startup, capacity), plan.audioServo) {}
    MediaAvSyncPlanView(const MediaAvSyncFixedFields& plan, MediaAvSyncStartupPolicyView startupPolicy,
        MediaAvSyncAudioServoPolicy servo)
        : members(plan.members), domainRole(plan.domainRole), sourceLifecycle(plan.sourceLifecycle),
            sourceClockMode(plan.sourceClockMode), controlGenerationPolicy(plan.controlGenerationPolicy),
            masterClockMode(plan.masterClockMode), canonicalTimeBaseNumerator(plan.canonicalTimeBaseNumerator),
            canonicalTimeBaseDenominator(plan.canonicalTimeBaseDenominator), startup(startupPolicy),
            audioServo(servo), video(plan.video), recovery(plan.recovery), rtpInput(plan.rtpInput),
            mpegTsInput(plan.mpegTsInput), demuxTimestampInput(plan.demuxTimestampInput),
            rtpOutput(plan.rtpOutput), projectMpegTsOutput(plan.projectMpegTsOutput), metrics(plan.metrics) {}
    MediaAvSyncPlanView(MediaAvSyncFixedPlan&&, const MediaAvSyncStartupCapacity&) = delete;
    MediaAvSyncPlanView(const MediaAvSyncFixedPlan&&, const MediaAvSyncStartupCapacity&) = delete;
    MediaAvSyncPlanView(MediaAvSyncFixedFields&&, MediaAvSyncStartupPolicyView, MediaAvSyncAudioServoPolicy) = delete;
    MediaAvSyncPlanView(const MediaAvSyncFixedFields&&, MediaAvSyncStartupPolicyView, MediaAvSyncAudioServoPolicy) = delete;
    MediaAvSyncPlanView(MediaAvSyncPlan&&) = delete;
    MediaAvSyncPlanView(const MediaAvSyncPlan&&) = delete;
    MediaAvSyncPlanView(MediaAvSyncPlan&&, MediaAvSyncStartupPolicyView, MediaAvSyncAudioServoPolicy) = delete;
    MediaAvSyncPlanView(const MediaAvSyncPlan&&, MediaAvSyncStartupPolicyView, MediaAvSyncAudioServoPolicy) = delete;
};
} // namespace media::ffmpeg::graph

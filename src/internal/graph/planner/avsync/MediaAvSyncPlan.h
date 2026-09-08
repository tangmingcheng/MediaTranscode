#pragma once

#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"

#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "internal/graph/model/MediaControlGenerationPolicy.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupPolicy.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/realtime/MediaPreparedGenericInputPlan.h"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaAvSyncMasterClockMode : std::uint8_t {
    SteadyMonotonic = 0
};

struct MediaAvSyncStartupPolicy {
    std::optional<bool> requireVideoKeyFrame;
    std::optional<bool> trimAudioToCommonStart;
    std::optional<MediaRunningTime> maximumWaitNs;
    std::optional<MediaRunningTime> prerollNs;
    std::optional<MediaRunningTime> keyFrameWaitNs;
    std::optional<MediaRunningTime> maximumAudioTrimNs;
    std::optional<MediaRunningTime> maximumInitialSkewNs;
    std::optional<MediaRunningTime> maximumGapNs;
    std::optional<MediaRunningTime> outputLeadNs;
    std::optional<std::size_t> videoCapacity;
    std::optional<std::size_t> audioCapacity;
    std::optional<std::uint64_t> videoByteCapacity;
    std::optional<std::uint64_t> audioByteCapacity;
    std::optional<std::uint64_t> maximumVideoUnitBytes;
    std::optional<std::uint64_t> maximumAudioUnitBytes;
    std::optional<std::string> videoIdentity;
    std::optional<std::string> audioIdentity;
    std::optional<bool> allowDegradedClock;
};

enum class MediaAudioServoAntiWindupMode : std::uint8_t {
    ConditionalIntegration = 0
};

struct MediaAvSyncAudioServoPolicy {
    std::optional<MediaRunningTime> deadbandNs;
    std::optional<MediaRunningTime> phaseFilterTimeConstantNs;
    std::optional<MediaRunningTime> frequencyFilterTimeConstantNs;
    std::optional<int> proportionalGainPpmPerSecond;
    std::optional<int> integralGainPpmPerSecondSquared;
    std::optional<int> integratorLimitPpm;
    std::optional<int> frequencyFeedForwardNumerator;
    std::optional<int> frequencyFeedForwardDenominator;
    std::optional<int> frequencyDeadbandPpm;
    std::optional<int> maximumMeasuredFrequencyPpm;
    std::optional<int> recoveryExitFrequencyPpm;
    std::optional<MediaAudioServoAntiWindupMode> antiWindupMode;
    std::optional<MediaRunningTime> minimumUpdateIntervalNs;
    std::optional<MediaRunningTime> maximumMeasurementGapNs;
    std::optional<int> maximumSlewPpmPerSecond;
    std::optional<int> normalCorrectionLimitPpm;
    std::optional<int> recoveryCorrectionLimitPpm;
    std::optional<MediaRunningTime> recoveryEnterThresholdNs;
    std::optional<MediaRunningTime> recoveryExitThresholdNs;
    std::optional<MediaRunningTime> recoveryExitHoldNs;
    std::optional<MediaRunningTime> compensationWindowNs;
    std::optional<MediaRunningTime> commandLeadNs;
    std::optional<int> outputSampleRate;
    std::optional<std::size_t> correctionLookaheadWindows;
};

struct MediaAvSyncVideoPolicy {
    std::optional<MediaRunningTime> earlyHoldThresholdNs;
    std::optional<MediaRunningTime> lateDisplayThresholdNs;
    std::optional<MediaRunningTime> dropThresholdNs;
    std::optional<bool> allowRecoveryRepeat;
    std::optional<int> maximumConsecutiveRecoveryActions;
};

struct MediaAvSyncRecoveryPolicy {
    std::optional<MediaRunningTime> suspectThresholdNs;
    std::optional<MediaRunningTime> hardDiscontinuityThresholdNs;
    std::optional<MediaRunningTime> reacquisitionTimeoutNs;
};

struct MediaAvSyncRtpInputStreamPlan {
    std::optional<std::string> identity;
    std::optional<int> payloadType;
    std::optional<int> clockRate;
};

enum class MediaAvSyncRtpStreamAssociationMode : std::uint8_t {
    Unknown = 0,
    PlannedStreamPair = 1
};

struct MediaAvSyncRtpInputPolicy {
    MediaRtpCommonEpochPolicy commonEpochPolicy =
        MediaRtpCommonEpochPolicy::Unknown;
    MediaAvSyncRtpStreamAssociationMode streamAssociationMode =
        MediaAvSyncRtpStreamAssociationMode::Unknown;
    std::optional<MediaRtcpCompositionMode> rtcpCompositionMode;
    std::optional<MediaRunningTime> identityEvidenceTimeoutNs;
    std::optional<bool> requireSenderReports;
    std::optional<MediaRunningTime> senderReportTimeoutNs;
    std::optional<MediaRunningTime> maximumExtrapolationNs;
    std::optional<MediaRtpClockLossPolicy> clockLossPolicy;
    std::optional<MediaRtpClockLossPolicy> secondaryClockLossPolicy;
    std::optional<MediaRunningTime> maximumInterStreamClockOffsetSkewNs;
    std::optional<int> maximumSenderClockRateErrorPpm;
    std::optional<MediaRunningTime> maximumSenderClockResidualNs;
};

struct MediaAvSyncRtpOutputStreamPlan {
    std::optional<std::string> identity;
    std::optional<int> payloadType;
    std::optional<int> clockRate;
    std::optional<std::uint32_t> ssrc;
    std::optional<std::uint32_t> baseTimestamp;
    std::optional<std::string> cname;
};

struct MediaAvSyncRtpOutputPolicy {
    std::optional<bool> useSharedNtpEpoch;
};

struct MediaAvSyncRtpInputPlan {
    MediaAvSyncRtpInputStreamPlan videoInput;
    MediaAvSyncRtpInputStreamPlan audioInput;
    MediaAvSyncRtpInputPolicy input;
};

struct MediaAvSyncRtpOutputPlan {
    MediaAvSyncRtpOutputStreamPlan videoOutput;
    MediaAvSyncRtpOutputStreamPlan audioOutput;
    MediaAvSyncRtpOutputPolicy output;
};

struct MediaAvSyncMpegTsInputPlan {
    std::optional<int> programNumber;
    std::optional<int> programMapPid;
    std::optional<int> videoPid;
    std::optional<int> audioPid;
    std::optional<int> pcrPid;
};

struct MediaAvSyncDemuxTimestampInputPlan {
    MediaRational videoTimeBase;
    MediaRational audioTimeBase;
    std::optional<MediaRunningTime> firstWindowMaximumSkewNs;
    std::optional<MediaRunningTime> discontinuityThresholdNs;
    std::optional<std::uint64_t> initialGeneration;
    std::optional<MediaRunningTime> canonicalTargetEpochNs;
    std::optional<MediaPreparedGenericInputPlan> preparedInput;
    std::optional<MediaPreparedGenericInputEvidence> preparedEvidence;
};

struct MediaAvSyncPreparedDemuxTimestampFacts {
    int videoStreamIndex;
    MediaRational videoTimeBase;
    int audioStreamIndex;
    MediaRational audioTimeBase;
    MediaPreparedGenericInputPlan preparedInput;
    MediaPreparedGenericInputEvidence preparedEvidence;
    MediaAvSyncStartupPolicy startup;
};

struct MediaAvSyncProjectMpegTsOutputPlan {
    std::optional<MediaTsMuxPlan> outputMux;
    std::optional<bool> useSharedNtpEpoch;
};

struct MediaAvSyncMetricsPolicy {
    std::optional<bool> collectStateAndGeneration;
    std::optional<bool> collectClockEvidence;
    std::optional<bool> collectQueueDurations;
    std::optional<bool> collectPhaseErrors;
    std::optional<bool> collectAudioCorrection;
    std::optional<bool> collectVideoRecoveryCounts;
    std::optional<bool> collectDiscontinuityCounts;
    std::optional<bool> collectProtocolClockHealth;
    std::optional<MediaRunningTime> maximumStartupSkewNs;
    std::optional<MediaRunningTime> maximumSteadyP95SkewNs;
    std::optional<MediaRunningTime> maximumSteadyP99SkewNs;
    std::optional<MediaRunningTime> maximumDriftNsPerHour;
};

struct MediaAvSyncPlan {
    std::optional<MediaAvSyncSourceClockMode> sourceClockMode;
    std::optional<MediaControlGenerationPolicy>
        controlGenerationPolicy;
    std::optional<MediaAvSyncMasterClockMode> masterClockMode;
    std::optional<int> canonicalTimeBaseNumerator;
    std::optional<int> canonicalTimeBaseDenominator;
    MediaAvSyncStartupPolicy startup;
    MediaAvSyncAudioServoPolicy audioServo;
    MediaAvSyncVideoPolicy video;
    MediaAvSyncRecoveryPolicy recovery;
    std::optional<MediaAvSyncRtpInputPlan> rtpInput;
    std::optional<MediaAvSyncMpegTsInputPlan> mpegTsInput;
    std::optional<MediaAvSyncDemuxTimestampInputPlan> demuxTimestampInput;
    std::optional<MediaAvSyncRtpOutputPlan> rtpOutput;
    std::optional<MediaAvSyncProjectMpegTsOutputPlan> projectMpegTsOutput;
    MediaAvSyncMetricsPolicy metrics;
};

} // namespace media::ffmpeg::graph

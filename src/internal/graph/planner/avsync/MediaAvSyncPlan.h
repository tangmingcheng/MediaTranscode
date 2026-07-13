#pragma once

#include "internal/graph/sync/MediaAvSyncTopology.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaAvSyncSourceClockMode : std::uint8_t {
    RtpSenderReports = 0,
    MpegTsPcr = 1
};

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
    std::optional<MediaRunningTime> outputLeadNs;
};

struct MediaAvSyncAudioServoPolicy {
    std::optional<MediaRunningTime> deadbandNs;
    std::optional<MediaRunningTime> shortControlWindowNs;
    std::optional<MediaRunningTime> longControlWindowNs;
    std::optional<int> proportionalGainPpm;
    std::optional<int> integralGainPpm;
    std::optional<int> maximumSlewPpmPerSecond;
    std::optional<int> normalCorrectionLimitPpm;
    std::optional<int> recoveryCorrectionLimitPpm;
    std::optional<MediaRunningTime> compensationWindowNs;
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

struct MediaAvSyncRtpInputPolicy {
    std::optional<bool> requireCommonCname;
    std::optional<bool> requireSenderReports;
    std::optional<MediaRunningTime> senderReportTimeoutNs;
    std::optional<MediaRunningTime> maximumExtrapolationNs;
    std::optional<MediaRunningTime> maximumSenderReportSkewNs;
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
    std::optional<MediaRunningTime> senderReportIntervalNs;
};

struct MediaAvSyncRtpPlan {
    MediaAvSyncRtpInputStreamPlan videoInput;
    MediaAvSyncRtpInputStreamPlan audioInput;
    MediaAvSyncRtpInputPolicy input;
    MediaAvSyncRtpOutputStreamPlan videoOutput;
    MediaAvSyncRtpOutputStreamPlan audioOutput;
    MediaAvSyncRtpOutputPolicy output;
};

struct MediaAvSyncTsPlan {
    std::optional<int> programNumber;
    std::optional<int> programMapPid;
    std::optional<int> videoPid;
    std::optional<int> audioPid;
    std::optional<int> pcrPid;
    std::optional<MediaRunningTime> pcrIntervalNs;
    std::optional<MediaRunningTime> maximumPcrGapNs;
    std::optional<MediaRunningTime> maximumPcrJitterNs;
    std::optional<int> timestampTimeBaseNumerator;
    std::optional<int> timestampTimeBaseDenominator;
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
    std::optional<MediaAvSyncTopology> topology;
    std::optional<MediaAvSyncSourceClockMode> sourceClockMode;
    std::optional<MediaAvSyncMasterClockMode> masterClockMode;
    std::optional<int> canonicalTimeBaseNumerator;
    std::optional<int> canonicalTimeBaseDenominator;
    MediaAvSyncStartupPolicy startup;
    MediaAvSyncAudioServoPolicy audioServo;
    MediaAvSyncVideoPolicy video;
    MediaAvSyncRecoveryPolicy recovery;
    std::optional<MediaAvSyncRtpPlan> rtp;
    std::optional<MediaAvSyncTsPlan> ts;
    MediaAvSyncMetricsPolicy metrics;
};

} // namespace media::ffmpeg::graph

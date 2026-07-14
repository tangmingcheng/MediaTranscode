#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include "internal/graph/sync/MediaAvSyncError.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaAudioDriftMeasurement final {
    MediaRunningTime phaseError;
    MediaRunningTime observedAt;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::int64_t effectiveOutputSampleIndex = 0;
    int outputSampleRate = 0;
};

class MediaAudioDriftServo final {
public:
    static MediaAvSyncResult<MediaAudioDriftServo> create(
        MediaAvSyncTopology topology,
        const MediaAvSyncAudioServoPolicy& policy,
        MediaRunningTime hardDiscontinuityThreshold,
        std::uint64_t generation);

    MediaAvSyncResult<MediaAudioServoDecision> update(
        const MediaAudioDriftMeasurement& measurement);
    MediaAvSyncStatus reset(std::uint64_t generation,
                            std::int64_t epochOutputSampleIndex);

private:
    struct Policy final {
        std::int64_t deadbandNs = 0;
        std::int64_t phaseFilterTimeConstantNs = 0;
        std::int64_t frequencyFilterTimeConstantNs = 0;
        int proportionalGainPpmPerSecond = 0;
        int integralGainPpmPerSecondSquared = 0;
        int integratorLimitPpm = 0;
        int frequencyFeedForwardNumerator = 0;
        int frequencyFeedForwardDenominator = 0;
        int frequencyDeadbandPpm = 0;
        int maximumMeasuredFrequencyPpm = 0;
        int recoveryExitFrequencyPpm = 0;
        std::int64_t minimumUpdateIntervalNs = 0;
        std::int64_t maximumMeasurementGapNs = 0;
        int maximumSlewPpmPerSecond = 0;
        int normalCorrectionLimitPpm = 0;
        int recoveryCorrectionLimitPpm = 0;
        std::int64_t recoveryEnterThresholdNs = 0;
        std::int64_t recoveryExitThresholdNs = 0;
        std::int64_t recoveryExitHoldNs = 0;
        std::int64_t compensationWindowNs = 0;
        std::int64_t commandLeadNs = 0;
        int outputSampleRate = 0;
        std::size_t correctionLookaheadWindows = 0;
        std::int64_t hardDiscontinuityThresholdNs = 0;
    };

    MediaAudioDriftServo(MediaAvSyncTopology topology,
                         Policy policy,
                         MediaAudioCorrectionQuantizer quantizer,
                         std::uint64_t generation) noexcept;

    MediaAvSyncResult<MediaAudioServoDecision> processCurrent(
        const MediaAudioDriftMeasurement& measurement,
        std::int64_t elapsedNs);
    MediaAvSyncResult<MediaAudioServoDecision> publishWindowIfDue(
        const MediaAudioDriftMeasurement& measurement,
        int stretchPpm);
    MediaAvSyncError error(MediaAvSyncErrorCode code,
                           const char* operation,
                           const MediaAudioDriftMeasurement* measurement,
                           const char* detail) const;
    void clearControlState() noexcept;
    MediaAvSyncStatus clearCorrectionEpoch(std::int64_t epochOutputSampleIndex);

    MediaAvSyncTopology m_topology;
    Policy m_policy;
    MediaAudioCorrectionQuantizer m_quantizer;
    int m_outputSampleRate = 0;
    std::uint64_t m_generation = 0;
    bool m_initialized = false;
    bool m_recovering = false;
    std::int64_t m_lastObservedAtNs = 0;
    std::int64_t m_lastRawPhaseNs = 0;
    std::int64_t m_filteredPhaseNs = 0;
    int m_filteredFrequencyPpm = 0;
    int m_integralPpm = 0;
    int m_lastStretchPpm = 0;
    std::int64_t m_recoveryExitHeldNs = 0;
    std::uint64_t m_lastSequence = 0;
    std::int64_t m_lastEffectiveOutputSampleIndex = 0;
};

} // namespace media::ffmpeg::graph

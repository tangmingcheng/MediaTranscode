#include "internal/graph/sync/MediaAudioDriftServo.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"
#include "internal/graph/sync/MediaAudioDriftServoPolicyValidator.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace media::ffmpeg::graph {
namespace {

template <typename T>
bool positive(const std::optional<T>& value) noexcept
{
    return value && *value > 0;
}

bool positive(const std::optional<MediaRunningTime>& value) noexcept
{
    return value && value->nanoseconds() > 0;
}

std::uint64_t magnitude(std::int64_t value) noexcept
{
    return value >= 0
        ? static_cast<std::uint64_t>(value)
        : static_cast<std::uint64_t>(-(value + 1)) + 1;
}

std::int64_t roundSignedRatio(std::int64_t numerator,
                              std::int64_t denominator) noexcept
{
    const std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    const std::int64_t half = (denominator + 1) / 2;
    if (remainder >= half) {
        return quotient + 1;
    }
    if (remainder <= -half) {
        return quotient - 1;
    }
    return quotient;
}

std::int64_t lowPass(std::int64_t previous,
                     std::int64_t sample,
                     std::int64_t elapsedNs,
                     std::int64_t timeConstantNs) noexcept
{
    const std::int64_t difference = sample - previous;
    return previous + roundSignedRatio(
        difference * elapsedNs, timeConstantNs + elapsedNs);
}

int clampInt64ToInt(std::int64_t value) noexcept
{
    return static_cast<int>(std::clamp(
        value,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

bool validPolicy(const MediaAvSyncAudioServoPolicy& policy,
                 MediaRunningTime hardDiscontinuityThreshold,
                 std::uint64_t generation) noexcept
{
    return generation > 0 && positive(policy.outputSampleRate) &&
           positive(policy.correctionLookaheadWindows) &&
           positive(policy.deadbandNs) &&
           positive(policy.phaseFilterTimeConstantNs) &&
           positive(policy.frequencyFilterTimeConstantNs) &&
           positive(policy.proportionalGainPpmPerSecond) &&
           positive(policy.integralGainPpmPerSecondSquared) &&
           positive(policy.integratorLimitPpm) &&
           policy.frequencyFeedForwardNumerator &&
           *policy.frequencyFeedForwardNumerator >= 0 &&
           positive(policy.frequencyFeedForwardDenominator) &&
           *policy.frequencyFeedForwardNumerator <=
               *policy.frequencyFeedForwardDenominator &&
           policy.frequencyDeadbandPpm && *policy.frequencyDeadbandPpm >= 0 &&
           positive(policy.maximumMeasuredFrequencyPpm) &&
           positive(policy.recoveryExitFrequencyPpm) &&
           *policy.frequencyDeadbandPpm < *policy.recoveryExitFrequencyPpm &&
           *policy.recoveryExitFrequencyPpm < *policy.maximumMeasuredFrequencyPpm &&
           policy.antiWindupMode &&
           *policy.antiWindupMode ==
               MediaAudioServoAntiWindupMode::ConditionalIntegration &&
           positive(policy.minimumUpdateIntervalNs) &&
           positive(policy.maximumMeasurementGapNs) &&
           positive(policy.maximumSlewPpmPerSecond) &&
           positive(policy.normalCorrectionLimitPpm) &&
           positive(policy.recoveryCorrectionLimitPpm) &&
           positive(policy.recoveryEnterThresholdNs) &&
           positive(policy.recoveryExitThresholdNs) &&
           positive(policy.recoveryExitHoldNs) &&
           positive(policy.compensationWindowNs) &&
           positive(policy.commandLeadNs) &&
           hardDiscontinuityThreshold.nanoseconds() > 0 &&
           *policy.minimumUpdateIntervalNs < *policy.phaseFilterTimeConstantNs &&
           *policy.phaseFilterTimeConstantNs < *policy.maximumMeasurementGapNs &&
           *policy.maximumMeasurementGapNs < *policy.frequencyFilterTimeConstantNs &&
           *policy.deadbandNs < *policy.recoveryExitThresholdNs &&
           *policy.recoveryExitThresholdNs < *policy.recoveryEnterThresholdNs &&
           *policy.recoveryEnterThresholdNs < hardDiscontinuityThreshold &&
           *policy.recoveryExitHoldNs >= *policy.minimumUpdateIntervalNs &&
           *policy.maximumMeasurementGapNs < *policy.commandLeadNs &&
           *policy.commandLeadNs < *policy.compensationWindowNs &&
           *policy.compensationWindowNs < *policy.frequencyFilterTimeConstantNs &&
           *policy.normalCorrectionLimitPpm <=
               MediaAudioDriftServoLimits::MaximumNormalCorrectionPpm &&
           *policy.recoveryCorrectionLimitPpm <=
               MediaAudioDriftServoLimits::MaximumRecoveryCorrectionPpm &&
           *policy.integratorLimitPpm <= *policy.recoveryCorrectionLimitPpm &&
           *policy.maximumSlewPpmPerSecond <=
               *policy.normalCorrectionLimitPpm &&
           *policy.normalCorrectionLimitPpm <=
               *policy.recoveryCorrectionLimitPpm &&
           *policy.outputSampleRate <=
               MediaAudioDriftServoLimits::MaximumOutputSampleRate &&
           *policy.correctionLookaheadWindows <=
               MediaAudioDriftServoLimits::MaximumCorrectionLookaheadWindows &&
           MediaAudioDriftServoPolicyValidator::leadCoversWorstPositiveGap(
               policy.commandLeadNs->nanoseconds(),
               policy.maximumMeasurementGapNs->nanoseconds(),
               *policy.outputSampleRate,
               *policy.recoveryCorrectionLimitPpm) &&
           hardDiscontinuityThreshold.nanoseconds() <=
               MediaAudioDriftServoLimits::MaximumHardDiscontinuityNs &&
           policy.maximumMeasurementGapNs->nanoseconds() <=
               MediaAudioDriftServoLimits::MaximumMeasurementGapNs &&
           policy.frequencyFilterTimeConstantNs->nanoseconds() <=
               MediaAudioDriftServoLimits::MaximumPolicyDurationNs &&
           policy.compensationWindowNs->nanoseconds() <=
               MediaAudioDriftServoLimits::MaximumPolicyDurationNs &&
           policy.recoveryExitHoldNs->nanoseconds() <=
               MediaAudioDriftServoLimits::MaximumPolicyDurationNs &&
           *policy.proportionalGainPpmPerSecond <=
               MediaAudioDriftServoLimits::MaximumProportionalGainPpmPerSecond &&
           *policy.integralGainPpmPerSecondSquared <=
               MediaAudioDriftServoLimits::MaximumIntegralGainPpmPerSecondSquared &&
           *policy.maximumMeasuredFrequencyPpm <=
               MediaAudioDriftServoLimits::MaximumMeasuredFrequencyPpm;
}

} // namespace

MediaAudioDriftServo::MediaAudioDriftServo(MediaAvSyncTopology topology,
                                           Policy policy,
                                           MediaAudioCorrectionQuantizer quantizer,
                                           std::uint64_t generation) noexcept
    : m_topology(topology)
    , m_policy(policy)
    , m_quantizer(std::move(quantizer))
    , m_outputSampleRate(policy.outputSampleRate)
    , m_generation(generation)
{
}

MediaAvSyncResult<MediaAudioDriftServo> MediaAudioDriftServo::create(
    MediaAvSyncTopology topology,
    const MediaAvSyncAudioServoPolicy& policy,
    MediaRunningTime hardDiscontinuityThreshold,
    std::uint64_t generation)
{
    if (!validPolicy(policy, hardDiscontinuityThreshold, generation)) {
        return MediaAvSyncResult<MediaAudioDriftServo>::failure(MediaAvSyncError(
            MediaAvSyncErrorCode::InvalidAudioServoPolicy, topology,
            MediaAvSyncErrorState::AudioServo, "create", "audio", "audio",
            generation, std::nullopt, std::nullopt,
            MediaRunningTime::fromNanoseconds(0),
            MediaRunningTime::fromNanoseconds(0),
            std::numeric_limits<std::int64_t>::min(),
            std::numeric_limits<std::int64_t>::max(),
            "incomplete or inconsistent planner-owned audio servo policy"));
    }

    auto quantizer = MediaAudioCorrectionQuantizer::create(
        *policy.compensationWindowNs, *policy.commandLeadNs,
        *policy.outputSampleRate);
    if (!quantizer) {
        return MediaAvSyncResult<MediaAudioDriftServo>::failure(MediaAvSyncError(
            MediaAvSyncErrorCode::InvalidAudioServoPolicy, topology,
            MediaAvSyncErrorState::AudioServo, "create", "audio", "audio",
            generation, std::nullopt, std::nullopt,
            MediaRunningTime::fromNanoseconds(0),
            MediaRunningTime::fromNanoseconds(0),
            std::numeric_limits<std::int64_t>::min(),
            std::numeric_limits<std::int64_t>::max(),
            "audio servo window and output sample rate are not jointly representable"));
    }

    Policy resolved;
    resolved.deadbandNs = policy.deadbandNs->nanoseconds();
    resolved.phaseFilterTimeConstantNs =
        policy.phaseFilterTimeConstantNs->nanoseconds();
    resolved.frequencyFilterTimeConstantNs =
        policy.frequencyFilterTimeConstantNs->nanoseconds();
    resolved.proportionalGainPpmPerSecond =
        *policy.proportionalGainPpmPerSecond;
    resolved.integralGainPpmPerSecondSquared =
        *policy.integralGainPpmPerSecondSquared;
    resolved.integratorLimitPpm = *policy.integratorLimitPpm;
    resolved.frequencyFeedForwardNumerator =
        *policy.frequencyFeedForwardNumerator;
    resolved.frequencyFeedForwardDenominator =
        *policy.frequencyFeedForwardDenominator;
    resolved.frequencyDeadbandPpm = *policy.frequencyDeadbandPpm;
    resolved.maximumMeasuredFrequencyPpm = *policy.maximumMeasuredFrequencyPpm;
    resolved.recoveryExitFrequencyPpm = *policy.recoveryExitFrequencyPpm;
    resolved.minimumUpdateIntervalNs =
        policy.minimumUpdateIntervalNs->nanoseconds();
    resolved.maximumMeasurementGapNs =
        policy.maximumMeasurementGapNs->nanoseconds();
    resolved.maximumSlewPpmPerSecond = *policy.maximumSlewPpmPerSecond;
    resolved.normalCorrectionLimitPpm = *policy.normalCorrectionLimitPpm;
    resolved.recoveryCorrectionLimitPpm = *policy.recoveryCorrectionLimitPpm;
    resolved.recoveryEnterThresholdNs =
        policy.recoveryEnterThresholdNs->nanoseconds();
    resolved.recoveryExitThresholdNs =
        policy.recoveryExitThresholdNs->nanoseconds();
    resolved.recoveryExitHoldNs = policy.recoveryExitHoldNs->nanoseconds();
    resolved.compensationWindowNs = policy.compensationWindowNs->nanoseconds();
    resolved.commandLeadNs = policy.commandLeadNs->nanoseconds();
    resolved.outputSampleRate = *policy.outputSampleRate;
    resolved.correctionLookaheadWindows = *policy.correctionLookaheadWindows;
    resolved.hardDiscontinuityThresholdNs =
        hardDiscontinuityThreshold.nanoseconds();
    return MediaAvSyncResult<MediaAudioDriftServo>::success(
        MediaAudioDriftServo(topology, resolved, std::move(quantizer).value(),
                             generation));
}

MediaAvSyncResult<MediaAudioServoDecision> MediaAudioDriftServo::update(
    const MediaAudioDriftMeasurement& measurement)
{
    if (measurement.generation < m_generation) {
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::dropOldGeneration(
                measurement.generation,
                measurement.sequence,
                measurement.effectiveOutputSampleIndex));
    }
    if (measurement.generation > m_generation) {
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::reacquire(
                measurement.generation,
                measurement.sequence,
                measurement.effectiveOutputSampleIndex,
                measurement.phaseError,
                0));
    }
    if (measurement.sequence == 0 || measurement.sequence <= m_lastSequence ||
        measurement.effectiveOutputSampleIndex < 0 ||
        (m_initialized && measurement.effectiveOutputSampleIndex <
                              m_lastEffectiveOutputSampleIndex) ||
        measurement.outputSampleRate != m_outputSampleRate) {
        return MediaAvSyncResult<MediaAudioServoDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidAudioServoMeasurement,
                  "update",
                  &measurement,
                  "sequence, sample index, or output sample rate contract failed"));
    }

    if (magnitude(measurement.phaseError.nanoseconds()) >=
        static_cast<std::uint64_t>(m_policy.hardDiscontinuityThresholdNs)) {
        clearControlState();
        if (auto status = clearCorrectionEpoch(
                measurement.effectiveOutputSampleIndex); !status) {
            return MediaAvSyncResult<MediaAudioServoDecision>::failure(status.error());
        }
        m_lastSequence = measurement.sequence;
        m_lastEffectiveOutputSampleIndex = measurement.effectiveOutputSampleIndex;
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::reacquire(
                measurement.generation,
                measurement.sequence,
                measurement.effectiveOutputSampleIndex,
                measurement.phaseError,
                0));
    }

    if (!m_initialized) {
        m_initialized = true;
        m_lastSourceEndOnMasterNs =
            measurement.sourceEndOnMaster.nanoseconds();
        m_lastRawPhaseNs = measurement.phaseError.nanoseconds();
        m_filteredPhaseNs = measurement.phaseError.nanoseconds();
        m_recovering = magnitude(m_filteredPhaseNs) >=
            static_cast<std::uint64_t>(m_policy.recoveryEnterThresholdNs);
        m_lastSequence = measurement.sequence;
        m_lastEffectiveOutputSampleIndex = measurement.effectiveOutputSampleIndex;
        return publishWindowIfDue(measurement, 0);
    }

    const auto elapsedResult = measurement.sourceEndOnMaster.checkedSubtract(
        MediaRunningTime::fromNanoseconds(m_lastSourceEndOnMasterNs));
    if (!elapsedResult || elapsedResult.value().nanoseconds() <= 0) {
        return MediaAvSyncResult<MediaAudioServoDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidAudioServoMeasurement,
                  "update",
                  &measurement,
                  "canonical source end must increase monotonically"));
    }
    const std::int64_t elapsedNs = elapsedResult.value().nanoseconds();
    if (elapsedNs < m_policy.minimumUpdateIntervalNs) {
        m_lastSequence = measurement.sequence;
        m_lastEffectiveOutputSampleIndex = measurement.effectiveOutputSampleIndex;
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::none(
                m_generation,
                measurement.sequence,
                measurement.effectiveOutputSampleIndex,
                MediaRunningTime::fromNanoseconds(m_filteredPhaseNs),
                m_filteredFrequencyPpm,
                m_integralPpm,
                m_recovering));
    }
    if (elapsedNs > m_policy.maximumMeasurementGapNs) {
        clearControlState();
        if (auto status = clearCorrectionEpoch(
                measurement.effectiveOutputSampleIndex); !status) {
            return MediaAvSyncResult<MediaAudioServoDecision>::failure(status.error());
        }
        m_lastSequence = measurement.sequence;
        m_lastEffectiveOutputSampleIndex = measurement.effectiveOutputSampleIndex;
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::reacquire(
                m_generation,
                measurement.sequence,
                measurement.effectiveOutputSampleIndex,
                measurement.phaseError,
                0));
    }
    const std::int64_t nominalSamples =
        (elapsedNs / MediaAudioDriftServoLimits::NanosecondsPerSecond) *
            m_outputSampleRate +
        ((elapsedNs % MediaAudioDriftServoLimits::NanosecondsPerSecond) *
             m_outputSampleRate +
         MediaAudioDriftServoLimits::NanosecondsPerSecond - 1) /
            MediaAudioDriftServoLimits::NanosecondsPerSecond;
    const std::int64_t maximumAdvance =
        nominalSamples +
        (nominalSamples * m_policy.recoveryCorrectionLimitPpm +
         MediaAudioDriftServoLimits::PartsPerMillion - 1) /
            MediaAudioDriftServoLimits::PartsPerMillion +
        2;
    if (measurement.effectiveOutputSampleIndex -
            m_lastEffectiveOutputSampleIndex >
        maximumAdvance) {
        return MediaAvSyncResult<MediaAudioServoDecision>::failure(
            error(MediaAvSyncErrorCode::InvalidAudioServoMeasurement,
                  "update", &measurement,
                  "output sample index advanced beyond canonical source elapsed-time bound"));
    }
    return processCurrent(measurement, elapsedNs);
}

MediaAvSyncResult<MediaAudioServoDecision> MediaAudioDriftServo::processCurrent(
    const MediaAudioDriftMeasurement& measurement,
    std::int64_t elapsedNs)
{
    const std::int64_t rawFrequencyPpm = roundSignedRatio(
        (measurement.phaseError.nanoseconds() - m_lastRawPhaseNs) *
            MediaAudioDriftServoLimits::PartsPerMillion,
        elapsedNs);
    if (magnitude(rawFrequencyPpm) >
        static_cast<std::uint64_t>(m_policy.maximumMeasuredFrequencyPpm)) {
        clearControlState();
        if (auto status = clearCorrectionEpoch(
                measurement.effectiveOutputSampleIndex); !status) {
            return MediaAvSyncResult<MediaAudioServoDecision>::failure(status.error());
        }
        m_lastSequence = measurement.sequence;
        m_lastEffectiveOutputSampleIndex = measurement.effectiveOutputSampleIndex;
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::reacquire(
                m_generation,
                measurement.sequence,
                measurement.effectiveOutputSampleIndex,
                measurement.phaseError,
                clampInt64ToInt(rawFrequencyPpm)));
    }

    m_filteredPhaseNs = lowPass(m_filteredPhaseNs,
                                measurement.phaseError.nanoseconds(),
                                elapsedNs,
                                m_policy.phaseFilterTimeConstantNs);
    m_filteredFrequencyPpm = clampInt64ToInt(lowPass(
        m_filteredFrequencyPpm,
        rawFrequencyPpm,
        elapsedNs,
        m_policy.frequencyFilterTimeConstantNs));

    if (!m_recovering && magnitude(measurement.phaseError.nanoseconds()) >=
        static_cast<std::uint64_t>(m_policy.recoveryEnterThresholdNs)) {
        m_recovering = true;
        m_recoveryExitHeldNs = 0;
    } else if (m_recovering) {
        const bool belowExit =
            magnitude(m_filteredPhaseNs) <=
                static_cast<std::uint64_t>(m_policy.recoveryExitThresholdNs) &&
            magnitude(m_filteredFrequencyPpm) <=
                static_cast<std::uint64_t>(m_policy.recoveryExitFrequencyPpm);
        m_recoveryExitHeldNs = belowExit
            ? std::min(m_policy.recoveryExitHoldNs,
                       m_recoveryExitHeldNs + elapsedNs)
            : 0;
        if (m_recoveryExitHeldNs >= m_policy.recoveryExitHoldNs) {
            m_recovering = false;
            m_recoveryExitHeldNs = 0;
        }
    }

    const std::int64_t phaseForControl =
        magnitude(m_filteredPhaseNs) <=
                static_cast<std::uint64_t>(m_policy.deadbandNs)
            ? 0
            : m_filteredPhaseNs;
    const int frequencyForControl =
        magnitude(m_filteredFrequencyPpm) <=
                static_cast<std::uint64_t>(m_policy.frequencyDeadbandPpm)
            ? 0
            : m_filteredFrequencyPpm;
    const std::int64_t proportional = roundSignedRatio(
        phaseForControl *
            static_cast<std::int64_t>(m_policy.proportionalGainPpmPerSecond),
        MediaAudioDriftServoLimits::NanosecondsPerSecond);
    const std::int64_t frequency = roundSignedRatio(
        static_cast<std::int64_t>(frequencyForControl) *
            m_policy.frequencyFeedForwardNumerator,
        m_policy.frequencyFeedForwardDenominator);
    const std::int64_t integralRate = roundSignedRatio(
        phaseForControl *
            static_cast<std::int64_t>(m_policy.integralGainPpmPerSecondSquared),
        MediaAudioDriftServoLimits::NanosecondsPerSecond);
    const std::int64_t integralIncrement = roundSignedRatio(
        integralRate * elapsedNs,
        MediaAudioDriftServoLimits::NanosecondsPerSecond);
    const int activeLimit = m_recovering
        ? m_policy.recoveryCorrectionLimitPpm
        : m_policy.normalCorrectionLimitPpm;
    const std::int64_t candidateIntegral = std::clamp(
        static_cast<std::int64_t>(m_integralPpm) + integralIncrement,
        -static_cast<std::int64_t>(m_policy.integratorLimitPpm),
        static_cast<std::int64_t>(m_policy.integratorLimitPpm));
    const std::int64_t candidateTarget = proportional + frequency + candidateIntegral;
    const bool windsFurther =
        (candidateTarget > activeLimit && integralIncrement > 0) ||
        (candidateTarget < -activeLimit && integralIncrement < 0);
    if (!windsFurther) {
        m_integralPpm = static_cast<int>(candidateIntegral);
    }

    const std::int64_t unclamped = proportional + frequency + m_integralPpm;
    const int target = static_cast<int>(std::clamp(
        unclamped,
        -static_cast<std::int64_t>(activeLimit),
        static_cast<std::int64_t>(activeLimit)));
    const std::int64_t slew = std::max<std::int64_t>(
        1,
        roundSignedRatio(
            static_cast<std::int64_t>(m_policy.maximumSlewPpmPerSecond) * elapsedNs,
            MediaAudioDriftServoLimits::NanosecondsPerSecond));
    const int stretchPpm = static_cast<int>(std::clamp(
        static_cast<std::int64_t>(target),
        static_cast<std::int64_t>(m_lastStretchPpm) - slew,
        static_cast<std::int64_t>(m_lastStretchPpm) + slew));

    m_lastSourceEndOnMasterNs = measurement.sourceEndOnMaster.nanoseconds();
    m_lastRawPhaseNs = measurement.phaseError.nanoseconds();
    m_lastSequence = measurement.sequence;
    m_lastEffectiveOutputSampleIndex = measurement.effectiveOutputSampleIndex;
    m_lastStretchPpm = stretchPpm;
    return publishWindowIfDue(measurement, stretchPpm);
}

MediaAvSyncResult<MediaAudioServoDecision>
MediaAudioDriftServo::publishWindowIfDue(
    const MediaAudioDriftMeasurement& measurement,
    int stretchPpm)
{
    auto correction = m_quantizer.schedule(
        m_generation,
        measurement.sequence,
        measurement.effectiveOutputSampleIndex,
        stretchPpm,
        MediaAudioCorrectionTelemetry{
            MediaRunningTime::fromNanoseconds(m_filteredPhaseNs),
            m_filteredFrequencyPpm,
            m_integralPpm,
            m_recovering});
    if (!correction) {
        return MediaAvSyncResult<MediaAudioServoDecision>::failure(
            error(MediaAvSyncErrorCode::AudioCorrectionQuantizationFailed,
                  "schedule",
                  &measurement,
                  "quantized correction contract failed"));
    }
    if (!correction.value()) {
        return MediaAvSyncResult<MediaAudioServoDecision>::success(
            MediaAudioServoDecision::none(
                m_generation, measurement.sequence,
                measurement.effectiveOutputSampleIndex,
                MediaRunningTime::fromNanoseconds(m_filteredPhaseNs),
                m_filteredFrequencyPpm, m_integralPpm, m_recovering));
    }
    return MediaAvSyncResult<MediaAudioServoDecision>::success(
        MediaAudioServoDecision::apply(std::move(*correction.value())));
}

MediaAvSyncStatus MediaAudioDriftServo::reset(
    std::uint64_t generation,
    std::int64_t epochOutputSampleIndex)
{
    if (generation == 0 || epochOutputSampleIndex < 0) {
        return MediaAvSyncStatus::failure(
            error(MediaAvSyncErrorCode::InvalidGenerationTransition,
                  "reset", nullptr,
                  "audio servo reset requires positive generation and non-negative epoch"));
    }
    m_generation = generation;
    clearControlState();
    return clearCorrectionEpoch(epochOutputSampleIndex);
}

MediaAvSyncStatus MediaAudioDriftServo::clearCorrectionEpoch(
    std::int64_t epochOutputSampleIndex)
{
    if (auto status = m_quantizer.resetEpoch(epochOutputSampleIndex); !status) {
        return MediaAvSyncStatus::failure(
            error(MediaAvSyncErrorCode::AudioCorrectionQuantizationFailed,
                  "reset_epoch", nullptr,
                  "audio correction epoch sample index is invalid"));
    }
    return MediaAvSyncStatus::success();
}

void MediaAudioDriftServo::clearControlState() noexcept
{
    m_initialized = false;
    m_recovering = false;
    m_lastSourceEndOnMasterNs = 0;
    m_lastRawPhaseNs = 0;
    m_filteredPhaseNs = 0;
    m_filteredFrequencyPpm = 0;
    m_integralPpm = 0;
    m_lastStretchPpm = 0;
    m_recoveryExitHeldNs = 0;
    m_lastSequence = 0;
    m_lastEffectiveOutputSampleIndex = 0;
}

MediaAvSyncError MediaAudioDriftServo::error(
    MediaAvSyncErrorCode code,
    const char* operation,
    const MediaAudioDriftMeasurement* measurement,
    const char* detail) const
{
    return MediaAvSyncError(
        code,
        m_topology,
        MediaAvSyncErrorState::AudioServo,
        operation,
        "audio",
        "audio",
        m_generation,
        measurement ? std::optional<std::uint64_t>(measurement->generation)
                    : std::nullopt,
        measurement
            ? std::optional<MediaRunningTime>(measurement->phaseError)
            : std::nullopt,
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0),
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
        detail);
}

} // namespace media::ffmpeg::graph

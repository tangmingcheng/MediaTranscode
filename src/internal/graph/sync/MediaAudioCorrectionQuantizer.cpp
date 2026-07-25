#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::int64_t roundSignedRatio(std::int64_t numerator,
                              std::int64_t denominator) noexcept
{
    if (numerator >= 0) {
        return numerator / denominator +
               ((numerator % denominator) >= (denominator + 1) / 2 ? 1 : 0);
    }
    const std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    return quotient -
           (-remainder >= (denominator + 1) / 2 ? 1 : 0);
}

} // namespace

MediaAudioCorrectionQuantizer::MediaAudioCorrectionQuantizer(
    std::int64_t windowNs,
    std::int64_t leadSamples,
    int outputSampleRate) noexcept
    : m_windowNs(windowNs)
    , m_outputSampleRate(outputSampleRate)
    , m_leadSamples(leadSamples)
{
}

::media::Result<MediaAudioCorrectionQuantizer>
MediaAudioCorrectionQuantizer::create(MediaRunningTime compensationWindow,
                                      MediaRunningTime commandLead,
                                      int outputSampleRate)
{
    const std::int64_t windowNs = compensationWindow.nanoseconds();
    const std::int64_t leadNs = commandLead.nanoseconds();
    if (windowNs <= 0 || leadNs <= 0 || leadNs >= windowNs ||
        outputSampleRate <= 0 ||
        windowNs > (std::numeric_limits<std::int64_t>::max() -
                    MediaAudioDriftServoLimits::NanosecondsPerSecond) / outputSampleRate) {
        return ::media::Result<MediaAudioCorrectionQuantizer>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction window product is not representable"));
    }
    const std::int64_t product = windowNs * outputSampleRate;
    const std::int64_t leadProduct = leadNs * outputSampleRate;
    if (product < MediaAudioDriftServoLimits::NanosecondsPerSecond ||
        product / MediaAudioDriftServoLimits::NanosecondsPerSecond >
            std::numeric_limits<int>::max()) {
        return ::media::Result<MediaAudioCorrectionQuantizer>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction window must represent 1..INT_MAX samples"));
    }
    const std::int64_t leadSamples =
        (leadProduct + MediaAudioDriftServoLimits::NanosecondsPerSecond - 1) /
        MediaAudioDriftServoLimits::NanosecondsPerSecond;
    if (leadSamples <= 0 ||
        leadSamples >= product / MediaAudioDriftServoLimits::NanosecondsPerSecond) {
        return ::media::Result<MediaAudioCorrectionQuantizer>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction lead must be inside the planned window"));
    }
    return ::media::Result<MediaAudioCorrectionQuantizer>::success(
        MediaAudioCorrectionQuantizer(windowNs, leadSamples, outputSampleRate));
}

::media::Result<std::optional<MediaAudioCompensationCommand>>
MediaAudioCorrectionQuantizer::schedule(
    std::uint64_t generation,
    std::uint64_t sequence,
    std::int64_t observedOutputSampleIndex,
    int stretchPpm,
    const MediaAudioCorrectionTelemetry& telemetry)
{
    if (generation == 0 || sequence == 0 || observedOutputSampleIndex < 0 ||
        stretchPpm <= -MediaAudioDriftServoLimits::PartsPerMillion) {
        return ::media::Result<std::optional<MediaAudioCompensationCommand>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction schedule inputs are invalid"));
    }
    if (m_epochHasPublishedWindow && m_nextPlannedEnd &&
        observedOutputSampleIndex < *m_nextPlannedEnd - m_leadSamples) {
        return ::media::Result<std::optional<MediaAudioCompensationCommand>>::success(
            std::nullopt);
    }

    const std::int64_t effective = m_nextPlannedEnd.value_or(
        observedOutputSampleIndex);
    const std::int64_t windowNumerator =
        m_windowNs * m_outputSampleRate + m_windowRemainder;
    const std::int64_t nominal = roundSignedRatio(
        windowNumerator, MediaAudioDriftServoLimits::NanosecondsPerSecond);
    const std::int64_t nextWindowRemainder =
        windowNumerator -
        nominal * MediaAudioDriftServoLimits::NanosecondsPerSecond;
    if (nominal <= 0 || nominal > std::numeric_limits<int>::max() ||
        nominal > (std::numeric_limits<std::int64_t>::max() -
                   MediaAudioDriftServoLimits::PartsPerMillion) /
            std::max(1, std::abs(stretchPpm))) {
        return ::media::Result<std::optional<MediaAudioCompensationCommand>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction nominal window is not representable"));
    }
    const std::int64_t deltaNumerator =
        nominal * static_cast<std::int64_t>(stretchPpm) + m_deltaRemainder;
    const std::int64_t delta = roundSignedRatio(
        deltaNumerator, MediaAudioDriftServoLimits::PartsPerMillion);
    const std::int64_t nextDeltaRemainder =
        deltaNumerator - delta * MediaAudioDriftServoLimits::PartsPerMillion;
    const std::int64_t distance = nominal + delta;
    if (delta < std::numeric_limits<int>::min() ||
        delta > std::numeric_limits<int>::max() || distance <= 0 ||
        distance > std::numeric_limits<int>::max() ||
        effective > std::numeric_limits<std::int64_t>::max() - distance) {
        return ::media::Result<std::optional<MediaAudioCompensationCommand>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction command or sample index is not representable"));
    }

    MediaAudioCompensationCommand correction(
        generation,
        sequence,
        effective,
        stretchPpm,
        static_cast<int>(delta),
        static_cast<int>(distance),
        telemetry.filteredPhaseError,
        telemetry.filteredFrequencyPpm,
        telemetry.integralPpm,
        telemetry.recovering);
    m_windowRemainder = nextWindowRemainder;
    m_deltaRemainder = nextDeltaRemainder;
    m_nextPlannedEnd = effective + distance;
    m_epochHasPublishedWindow = true;
    return ::media::Result<std::optional<MediaAudioCompensationCommand>>::success(
        std::move(correction));
}

::media::Status MediaAudioCorrectionQuantizer::resetEpoch(
    std::int64_t epochOutputSampleIndex)
{
    if (epochOutputSampleIndex < 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "audio correction epoch sample index is negative"));
    }
    m_nextPlannedEnd = epochOutputSampleIndex;
    m_epochHasPublishedWindow = false;
    m_windowRemainder = 0;
    m_deltaRemainder = 0;
    return ::media::Status::success();
}

std::optional<std::int64_t>
MediaAudioCorrectionQuantizer::nextPlannedEnd() const noexcept
{
    return m_nextPlannedEnd;
}

} // namespace media::ffmpeg::graph

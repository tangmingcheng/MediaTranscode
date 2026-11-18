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

struct WindowFacts final {
    std::int64_t nanoseconds;
    std::int64_t leadSamples;
    int sampleRate;
};
using Rejection = MediaAudioCorrectionCapacityRejection;
using Position = MediaAudioCorrectionCapacityPosition;
using WindowAssessment = std::variant<WindowFacts, Rejection>;
enum class WindowMode { RuntimeContract, CapacityDomain };

::media::Result<WindowAssessment> assessWindow(
    MediaRunningTime compensationWindow, MediaRunningTime commandLead, int outputSampleRate, WindowMode mode)
{
    using Result = ::media::Result<WindowAssessment>;
    const auto reject = [](Position position, const char* message) {
        return Result::success(Rejection{position, ::media::ErrorInfo::invalidArgument(message)});
    };
    const auto ns = MediaAudioDriftServoLimits::NanosecondsPerSecond;
    const auto windowNs = compensationWindow.nanoseconds();
    const auto leadNs = commandLead.nanoseconds();
    if (windowNs <= 0 || leadNs <= 0 || leadNs > windowNs || outputSampleRate <= 0 ||
        (leadNs == windowNs && mode == WindowMode::RuntimeContract))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "audio correction window product is not representable"));
    if (windowNs > (std::numeric_limits<std::int64_t>::max() - ns) / outputSampleRate)
        return reject(Position::UpperBound, "audio correction window product is not representable");
    const auto product = windowNs * outputSampleRate;
    const auto leadProduct = leadNs * outputSampleRate;
    if (product < ns)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "audio correction window must represent 1..INT_MAX samples"));
    if (product / ns > std::numeric_limits<int>::max())
        return reject(Position::UpperBound, "audio correction window must represent 1..INT_MAX samples");
    // At rates above one sample/ns, distinct sample counts may round to the
    // same nanosecond. It is a capacity-domain hole, not a fixed rate error.
    if (leadNs == windowNs)
        return reject(Position::QuantizationGap, "audio correction window product is not representable");
    const auto leadSamples = (leadProduct + ns - 1) / ns;
    if (leadSamples <= 0 || leadSamples >= product / ns)
        return reject(Position::QuantizationGap, "audio correction lead must be inside the planned window");
    return Result::success(WindowFacts{windowNs, leadSamples, outputSampleRate});
}

::media::Result<MediaAudioCorrectionCapacityAssessment> assessDistance(
    std::int64_t windowNs, int outputSampleRate, int maximumStretchPpm)
{
    using Result = ::media::Result<MediaAudioCorrectionCapacityAssessment>;
    // Called only after the window product is validated, including gap results.
    // Positive window rounding carries a residual below half a sample.
    // Signed delta rounding can retain positive half a sample after a negative tie.
    const auto ns = MediaAudioDriftServoLimits::NanosecondsPerSecond;
    const auto ppm = MediaAudioDriftServoLimits::PartsPerMillion;
    const auto product = windowNs * outputSampleRate;
    const auto nominal = product / ns + (product % ns != 0);
    if (maximumStretchPpm < 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "audio compensation distance bound is not representable"));
    if (nominal > std::numeric_limits<int>::max() ||
        (maximumStretchPpm != 0 && nominal >
            (std::numeric_limits<std::int64_t>::max() - ppm) / maximumStretchPpm))
        return Result::success(Rejection{Position::UpperBound, ::media::ErrorInfo::invalidArgument(
            "audio compensation distance bound is not representable")});
    const auto deltaProduct = nominal * maximumStretchPpm;
    const auto distance = nominal + roundSignedRatio(deltaProduct + ppm / 2, ppm);
    if (distance <= 0 || distance > std::numeric_limits<int>::max())
        return Result::success(Rejection{Position::UpperBound, ::media::ErrorInfo::invalidArgument(
            "audio compensation distance bound exceeds FFmpeg capacity")});
    return Result::success(static_cast<int>(distance));
}
} // namespace

MediaAudioCorrectionQuantizer::MediaAudioCorrectionQuantizer(
    std::int64_t windowNs, std::int64_t leadSamples, int outputSampleRate) noexcept
    : m_windowNs(windowNs), m_outputSampleRate(outputSampleRate), m_leadSamples(leadSamples)
{
}

::media::Result<MediaAudioCorrectionQuantizer> MediaAudioCorrectionQuantizer::create(
    MediaRunningTime compensationWindow, MediaRunningTime commandLead, int outputSampleRate)
{
    using Result = ::media::Result<MediaAudioCorrectionQuantizer>;
    auto assessed = assessWindow(compensationWindow, commandLead, outputSampleRate, WindowMode::RuntimeContract);
    if (!assessed) return Result::failure(assessed.error());
    if (const auto* rejected = std::get_if<Rejection>(&assessed.value()))
        return Result::failure(rejected->error);
    const auto& window = std::get<WindowFacts>(assessed.value());
    return Result::success(MediaAudioCorrectionQuantizer(window.nanoseconds, window.leadSamples, window.sampleRate));
}

::media::Result<int> MediaAudioCorrectionQuantizer::maximumCompensationDistance(int maximumStretchPpm) const
{
    auto assessed = assessDistance(m_windowNs, m_outputSampleRate, maximumStretchPpm);
    if (!assessed) return ::media::Result<int>::failure(assessed.error());
    if (const auto* rejected = std::get_if<Rejection>(&assessed.value()))
        return ::media::Result<int>::failure(rejected->error);
    return ::media::Result<int>::success(std::get<int>(assessed.value()));
}

::media::Result<MediaAudioCorrectionCapacityAssessment> MediaAudioCorrectionQuantizer::assessCapacity(
    MediaRunningTime compensationWindow, MediaRunningTime commandLead,
    int outputSampleRate, int maximumStretchPpm)
{
    using Result = ::media::Result<MediaAudioCorrectionCapacityAssessment>;
    if (maximumStretchPpm < 0) return Result::failure(::media::ErrorInfo::invalidArgument(
        "audio compensation distance bound is not representable"));
    auto window = assessWindow(compensationWindow, commandLead, outputSampleRate, WindowMode::CapacityDomain);
    if (!window) return Result::failure(window.error());
    const auto* rejected = std::get_if<Rejection>(&window.value());
    if (rejected && rejected->position == Position::UpperBound) return Result::success(*rejected);
    auto distance = assessDistance(compensationWindow.nanoseconds(), outputSampleRate, maximumStretchPpm);
    if (!distance) return distance;
    // A distance upper limit dominates a simultaneous quantization gap. Once
    // the window exceeds this limit, a later residue cannot restore validity.
    if (std::holds_alternative<Rejection>(distance.value())) return distance;
    return rejected ? Result::success(*rejected) : std::move(distance);
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

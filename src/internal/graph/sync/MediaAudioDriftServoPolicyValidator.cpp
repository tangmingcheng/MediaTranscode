#include "internal/graph/sync/MediaAudioDriftServoPolicyValidator.h"

#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include <limits>

namespace media::ffmpeg::graph {

bool MediaAudioDriftServoPolicyValidator::leadCoversWorstPositiveGap(
    std::int64_t leadNs,
    std::int64_t gapNs,
    int outputSampleRate,
    int maximumPositiveCorrectionPpm) noexcept
{
    if (leadNs <= 0 || gapNs <= 0 || outputSampleRate <= 0 ||
        maximumPositiveCorrectionPpm < 0 ||
        outputSampleRate > MediaAudioDriftServoLimits::MaximumOutputSampleRate ||
        gapNs > MediaAudioDriftServoLimits::MaximumMeasurementGapNs ||
        leadNs > MediaAudioDriftServoLimits::MaximumPolicyDurationNs) {
        return false;
    }

    const auto rate = static_cast<std::int64_t>(outputSampleRate);
    const auto correctionScale = MediaAudioDriftServoLimits::PartsPerMillion +
        static_cast<std::int64_t>(maximumPositiveCorrectionPpm);
    const auto leadLower = leadNs * rate /
        MediaAudioDriftServoLimits::NanosecondsPerSecond;

    const auto gapRateProduct = gapNs * rate;
    const auto wholeNominalSamples = gapRateProduct /
        MediaAudioDriftServoLimits::NanosecondsPerSecond;
    const auto nominalRemainder = gapRateProduct %
        MediaAudioDriftServoLimits::NanosecondsPerSecond;
    const auto scaledWhole = wholeNominalSamples * correctionScale;
    const auto wholeOutputSamples = scaledWhole /
        MediaAudioDriftServoLimits::PartsPerMillion;
    const auto scaledWholeRemainder = scaledWhole %
        MediaAudioDriftServoLimits::PartsPerMillion;
    const auto fractionalNumerator =
        scaledWholeRemainder * MediaAudioDriftServoLimits::NanosecondsPerSecond +
        nominalRemainder * correctionScale;
    constexpr auto FractionalDenominator =
        MediaAudioDriftServoLimits::NanosecondsPerSecond *
        MediaAudioDriftServoLimits::PartsPerMillion;
    const auto gapUpper = wholeOutputSamples +
        (fractionalNumerator + FractionalDenominator - 1) /
            FractionalDenominator;
    return leadLower >= gapUpper + 1;
}

} // namespace media::ffmpeg::graph

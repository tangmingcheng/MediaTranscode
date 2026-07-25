#pragma once

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaAudioDriftServoLimits final {
    static constexpr std::size_t MaximumCorrectionLookaheadWindows = 8;
    static constexpr int MaximumOutputSampleRate = 384'000;
    static constexpr std::int64_t MaximumMeasurementGapNs = 1'000'000'000;
    static constexpr std::int64_t MaximumPolicyDurationNs = 60'000'000'000;
    static constexpr std::int64_t MaximumHardDiscontinuityNs = 500'000'000;
    static constexpr int MaximumProportionalGainPpmPerSecond = 100'000;
    static constexpr int MaximumIntegralGainPpmPerSecondSquared = 100'000;
    static constexpr int MaximumMeasuredFrequencyPpm = 1'000'000;
    static constexpr int MaximumNormalCorrectionPpm = 1'000;
    static constexpr int MaximumRecoveryCorrectionPpm = 5'000;
    static constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
    static constexpr std::int64_t PartsPerMillion = 1'000'000;
};

} // namespace media::ffmpeg::graph

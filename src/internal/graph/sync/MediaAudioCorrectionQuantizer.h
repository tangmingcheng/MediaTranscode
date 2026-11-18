#pragma once

#include "internal/graph/sync/MediaAudioCorrection.h"

#include <cstdint>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaAudioCorrectionTelemetry final {
    MediaRunningTime filteredPhaseError;
    int filteredFrequencyPpm = 0;
    int integralPpm = 0;
    bool recovering = false;
};

// Internal capacity-domain result. A quantization gap is not a monotone
// upper bound and must never be interpreted as preparation budget pressure.
enum class MediaAudioCorrectionCapacityPosition { QuantizationGap, UpperBound };
struct MediaAudioCorrectionCapacityRejection final {
    MediaAudioCorrectionCapacityPosition position;
    ::media::ErrorInfo error;
};
// The positive int alternative is the maximum compensation distance.
using MediaAudioCorrectionCapacityAssessment =
    std::variant<int, MediaAudioCorrectionCapacityRejection>;

class MediaAudioCorrectionQuantizer final {
public:
    // Uses the same integer window and distance checks as create()/distance().
    // Invalid fixed inputs remain Result failures, not domain rejections.
    // UpperBound assumes fixed rate/stretch and a nondecreasing window.
    // Equal positive lead/window times are a domain gap after ns rounding.
    static ::media::Result<MediaAudioCorrectionCapacityAssessment> assessCapacity(
        MediaRunningTime compensationWindow, MediaRunningTime commandLead,
        int outputSampleRate, int maximumStretchPpm);

    static ::media::Result<MediaAudioCorrectionQuantizer> create(
        MediaRunningTime compensationWindow,
        MediaRunningTime commandLead,
        int outputSampleRate);

    ::media::Result<std::optional<MediaAudioCompensationCommand>> schedule(
        std::uint64_t generation,
        std::uint64_t sequence,
        std::int64_t observedOutputSampleIndex,
        int stretchPpm,
        const MediaAudioCorrectionTelemetry& telemetry);

    ::media::Result<int> maximumCompensationDistance(int maximumStretchPpm) const;

    ::media::Status resetEpoch(std::int64_t epochOutputSampleIndex);
    std::optional<std::int64_t> nextPlannedEnd() const noexcept;

private:
    MediaAudioCorrectionQuantizer(std::int64_t windowNs,
                                  std::int64_t leadSamples,
                                  int outputSampleRate) noexcept;

    std::int64_t m_windowNs = 0;
    int m_outputSampleRate = 0;
    std::int64_t m_leadSamples = 0;
    std::optional<std::int64_t> m_nextPlannedEnd;
    bool m_epochHasPublishedWindow = false;
    std::int64_t m_windowRemainder = 0;
    std::int64_t m_deltaRemainder = 0;
};

} // namespace media::ffmpeg::graph

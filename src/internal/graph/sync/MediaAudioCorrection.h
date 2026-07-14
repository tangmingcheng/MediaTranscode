#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaAudioCorrectionQuantizer;

enum class MediaAudioServoDecisionKind : std::uint8_t {
    None = 0,
    Apply = 1,
    Reacquire = 2,
    DropOldGeneration = 3
};

enum class MediaAudioCorrectionExecutionMode : std::uint8_t {
    Disabled = 0,
    ExternalCorrectionRequired = 1
};

const char* mediaAudioCorrectionExecutionModeName(
    MediaAudioCorrectionExecutionMode mode) noexcept;
::media::Result<MediaAudioCorrectionExecutionMode>
parseMediaAudioCorrectionExecutionMode(const std::string& value);

namespace MediaAudioCorrectionOptionKey {
inline constexpr const char* Mode = "audio_correction.mode";
inline constexpr const char* Generation = "audio_correction.generation";
inline constexpr const char* LookaheadWindows = "audio_correction.lookahead_windows";
}

class MediaAudioCompensationCommand final {
public:
    std::uint64_t generation() const noexcept { return m_generation; }
    std::uint64_t sequence() const noexcept { return m_sequence; }
    std::int64_t effectiveOutputSampleIndex() const noexcept
    {
        return m_effectiveOutputSampleIndex;
    }
    int stretchPpm() const noexcept { return m_stretchPpm; }
    int sampleDelta() const noexcept { return m_sampleDelta; }
    int compensationDistance() const noexcept { return m_compensationDistance; }
    MediaRunningTime filteredPhaseError() const noexcept
    {
        return m_filteredPhaseError;
    }
    int filteredFrequencyPpm() const noexcept { return m_filteredFrequencyPpm; }
    int integralPpm() const noexcept { return m_integralPpm; }
    bool recovering() const noexcept { return m_recovering; }

private:
    friend class MediaAudioCorrectionQuantizer;
    MediaAudioCompensationCommand(std::uint64_t generation,
                                  std::uint64_t sequence,
                                  std::int64_t effectiveOutputSampleIndex,
                                  int stretchPpm,
                                  int sampleDelta,
                                  int compensationDistance,
                                  MediaRunningTime filteredPhaseError,
                                  int filteredFrequencyPpm,
                                  int integralPpm,
                                  bool recovering) noexcept;

    std::uint64_t m_generation = 0;
    std::uint64_t m_sequence = 0;
    std::int64_t m_effectiveOutputSampleIndex = 0;
    int m_stretchPpm = 0;
    int m_sampleDelta = 0;
    int m_compensationDistance = 0;
    MediaRunningTime m_filteredPhaseError = MediaRunningTime::fromNanoseconds(0);
    int m_filteredFrequencyPpm = 0;
    int m_integralPpm = 0;
    bool m_recovering = false;
};

class MediaAudioServoDecision final {
public:
    static MediaAudioServoDecision none(std::uint64_t generation,
                                        std::uint64_t sequence,
                                        std::int64_t effectiveOutputSampleIndex,
                                        MediaRunningTime filteredPhaseError,
                                        int filteredFrequencyPpm,
                                        int integralPpm,
                                        bool recovering) noexcept;
    static MediaAudioServoDecision apply(MediaAudioCompensationCommand command) noexcept;
    static MediaAudioServoDecision reacquire(std::uint64_t generation,
                                             std::uint64_t sequence,
                                             std::int64_t effectiveOutputSampleIndex,
                                             MediaRunningTime filteredPhaseError,
                                             int filteredFrequencyPpm) noexcept;
    static MediaAudioServoDecision dropOldGeneration(
        std::uint64_t generation,
        std::uint64_t sequence,
        std::int64_t effectiveOutputSampleIndex) noexcept;

    MediaAudioServoDecisionKind kind() const noexcept { return m_kind; }
    std::uint64_t generation() const noexcept { return m_generation; }
    std::uint64_t sequence() const noexcept { return m_sequence; }
    std::int64_t effectiveOutputSampleIndex() const noexcept
    {
        return m_effectiveOutputSampleIndex;
    }
    MediaRunningTime filteredPhaseError() const noexcept
    {
        return m_filteredPhaseError;
    }
    int filteredFrequencyPpm() const noexcept { return m_filteredFrequencyPpm; }
    int integralPpm() const noexcept { return m_integralPpm; }
    bool recovering() const noexcept { return m_recovering; }
    const std::optional<MediaAudioCompensationCommand>& command() const noexcept
    {
        return m_command;
    }

private:
    MediaAudioServoDecision(MediaAudioServoDecisionKind kind,
                            std::uint64_t generation,
                            std::uint64_t sequence,
                            std::int64_t effectiveOutputSampleIndex,
                            MediaRunningTime filteredPhaseError,
                            int filteredFrequencyPpm,
                            int integralPpm,
                            bool recovering,
                            std::optional<MediaAudioCompensationCommand> command) noexcept;

    MediaAudioServoDecisionKind m_kind = MediaAudioServoDecisionKind::None;
    std::uint64_t m_generation = 0;
    std::uint64_t m_sequence = 0;
    std::int64_t m_effectiveOutputSampleIndex = 0;
    MediaRunningTime m_filteredPhaseError = MediaRunningTime::fromNanoseconds(0);
    int m_filteredFrequencyPpm = 0;
    int m_integralPpm = 0;
    bool m_recovering = false;
    std::optional<MediaAudioCompensationCommand> m_command;
};

} // namespace media::ffmpeg::graph

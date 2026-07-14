#include "internal/graph/sync/MediaAudioCorrection.h"

#include <utility>

namespace media::ffmpeg::graph {

const char* mediaAudioCorrectionExecutionModeName(
    MediaAudioCorrectionExecutionMode mode) noexcept
{
    switch (mode) {
    case MediaAudioCorrectionExecutionMode::Disabled:
        return "disabled";
    case MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired:
        return "external_required";
    }
    return "invalid";
}

::media::Result<MediaAudioCorrectionExecutionMode>
parseMediaAudioCorrectionExecutionMode(const std::string& value)
{
    if (value == mediaAudioCorrectionExecutionModeName(
                     MediaAudioCorrectionExecutionMode::Disabled)) {
        return ::media::Result<MediaAudioCorrectionExecutionMode>::success(
            MediaAudioCorrectionExecutionMode::Disabled);
    }
    if (value == mediaAudioCorrectionExecutionModeName(
                     MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired)) {
        return ::media::Result<MediaAudioCorrectionExecutionMode>::success(
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired);
    }
    return ::media::Result<MediaAudioCorrectionExecutionMode>::failure(
        ::media::ErrorInfo::invalidArgument(
            "unknown audio correction execution mode"));
}

MediaAudioCompensationCommand::MediaAudioCompensationCommand(
    std::uint64_t generation,
    std::uint64_t sequence,
    std::int64_t effectiveOutputSampleIndex,
    int stretchPpm,
    int sampleDelta,
    int compensationDistance,
    MediaRunningTime filteredPhaseError,
    int filteredFrequencyPpm,
    int integralPpm,
    bool recovering) noexcept
    : m_generation(generation)
    , m_sequence(sequence)
    , m_effectiveOutputSampleIndex(effectiveOutputSampleIndex)
    , m_stretchPpm(stretchPpm)
    , m_sampleDelta(sampleDelta)
    , m_compensationDistance(compensationDistance)
    , m_filteredPhaseError(filteredPhaseError)
    , m_filteredFrequencyPpm(filteredFrequencyPpm)
    , m_integralPpm(integralPpm)
    , m_recovering(recovering)
{
}

MediaAudioServoDecision::MediaAudioServoDecision(
    MediaAudioServoDecisionKind kind,
    std::uint64_t generation,
    std::uint64_t sequence,
    std::int64_t effectiveOutputSampleIndex,
    MediaRunningTime filteredPhaseError,
    int filteredFrequencyPpm,
    int integralPpm,
    bool recovering,
    std::optional<MediaAudioCompensationCommand> command) noexcept
    : m_kind(kind)
    , m_generation(generation)
    , m_sequence(sequence)
    , m_effectiveOutputSampleIndex(effectiveOutputSampleIndex)
    , m_filteredPhaseError(filteredPhaseError)
    , m_filteredFrequencyPpm(filteredFrequencyPpm)
    , m_integralPpm(integralPpm)
    , m_recovering(recovering)
    , m_command(std::move(command))
{
}

MediaAudioServoDecision MediaAudioServoDecision::none(
    std::uint64_t generation,
    std::uint64_t sequence,
    std::int64_t effectiveOutputSampleIndex,
    MediaRunningTime filteredPhaseError,
    int filteredFrequencyPpm,
    int integralPpm,
    bool recovering) noexcept
{
    return MediaAudioServoDecision(MediaAudioServoDecisionKind::None,
                                   generation,
                                   sequence,
                                   effectiveOutputSampleIndex,
                                   filteredPhaseError,
                                   filteredFrequencyPpm,
                                   integralPpm,
                                   recovering,
                                   std::nullopt);
}

MediaAudioServoDecision MediaAudioServoDecision::apply(
    MediaAudioCompensationCommand command) noexcept
{
    return MediaAudioServoDecision(MediaAudioServoDecisionKind::Apply,
                                   command.generation(),
                                   command.sequence(),
                                   command.effectiveOutputSampleIndex(),
                                   command.filteredPhaseError(),
                                   command.filteredFrequencyPpm(),
                                   command.integralPpm(),
                                   command.recovering(),
                                   std::move(command));
}

MediaAudioServoDecision MediaAudioServoDecision::reacquire(
    std::uint64_t generation,
    std::uint64_t sequence,
    std::int64_t effectiveOutputSampleIndex,
    MediaRunningTime filteredPhaseError,
    int filteredFrequencyPpm) noexcept
{
    return MediaAudioServoDecision(MediaAudioServoDecisionKind::Reacquire,
                                   generation,
                                   sequence,
                                   effectiveOutputSampleIndex,
                                   filteredPhaseError,
                                   filteredFrequencyPpm,
                                   0,
                                   false,
                                   std::nullopt);
}

MediaAudioServoDecision MediaAudioServoDecision::dropOldGeneration(
    std::uint64_t generation,
    std::uint64_t sequence,
    std::int64_t effectiveOutputSampleIndex) noexcept
{
    return MediaAudioServoDecision(MediaAudioServoDecisionKind::DropOldGeneration,
                                   generation,
                                   sequence,
                                   effectiveOutputSampleIndex,
                                   MediaRunningTime::fromNanoseconds(0),
                                   0,
                                   0,
                                   false,
                                   std::nullopt);
}

} // namespace media::ffmpeg::graph

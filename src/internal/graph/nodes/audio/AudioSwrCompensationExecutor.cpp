#include "internal/graph/nodes/audio/AudioSwrCompensationExecutor.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

extern "C" {
#include <libswresample/swresample.h>
}

#include <limits>

namespace media::ffmpeg::graph {

AudioSwrCompensationExecutor::AudioSwrCompensationExecutor(
    MediaAudioCorrectionExecutionMode mode,
    std::uint64_t generation,
    std::size_t lookaheadWindows) noexcept
    : m_mode(mode)
    , m_generation(generation)
    , m_lookaheadWindows(lookaheadWindows)
{
}

::media::Result<AudioSwrCompensationExecutor>
AudioSwrCompensationExecutor::create(MediaAudioCorrectionExecutionMode mode,
                                     std::uint64_t generation,
                                     std::size_t lookaheadWindows)
{
    const bool disabled = mode == MediaAudioCorrectionExecutionMode::Disabled;
    const bool external =
        mode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    if ((!disabled && !external) ||
        (disabled && (generation != 0 || lookaheadWindows != 0)) ||
        (external && (generation == 0 || lookaheadWindows == 0 ||
                      lookaheadWindows > MediaAudioDriftServoLimits::
                          MaximumCorrectionLookaheadWindows))) {
        return ::media::Result<AudioSwrCompensationExecutor>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction mode, generation, and lookahead are inconsistent"));
    }
    return ::media::Result<AudioSwrCompensationExecutor>::success(
        AudioSwrCompensationExecutor(mode, generation, lookaheadWindows));
}

::media::Status AudioSwrCompensationExecutor::enqueue(
    const MediaAudioCompensationCommand& command)
{
    if (m_mode != MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "disabled audio correction executor rejects commands"));
    }
    if (!canAccept()) {
        return ::media::Status::failure(::media::ErrorInfo::wouldBlock(
            "audio correction lookahead is full"));
    }
    const auto effective = command.effectiveOutputSampleIndex();
    const auto distance = command.compensationDistance();
    if (effective < 0 || distance <= 0 ||
        effective > std::numeric_limits<std::int64_t>::max() - distance) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "audio correction command window end overflow"));
    }
    const auto plannedEnd = effective + distance;
    if (command.generation() != m_generation || command.sequence() == 0 ||
        command.sequence() <= m_lastSequence ||
        effective < m_lastPlannedEnd ||
        (m_lastSequence != 0 &&
         effective != m_lastPlannedEnd)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "audio correction command violates generation, sequence, or window ordering"));
    }
    m_lastSequence = command.sequence();
    m_lastPlannedEnd = plannedEnd;
    m_pending.push_back(command);
    return ::media::Status::success();
}

::media::Result<AudioSwrCompensationWindow>
AudioSwrCompensationExecutor::prepare(SwrContext* swr,
                                      std::int64_t outputSampleIndex)
{
    if (outputSampleIndex < 0) {
        return ::media::Result<AudioSwrCompensationWindow>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio correction output sample index is negative"));
    }
    if (m_mode == MediaAudioCorrectionExecutionMode::Disabled) {
        return ::media::Result<AudioSwrCompensationWindow>::success(
            AudioSwrCompensationWindow{false, std::numeric_limits<int>::max()});
    }
    if (m_activeRemaining == 0) {
        m_active.reset();
        if (m_pending.empty() ||
            m_pending.front().effectiveOutputSampleIndex() != outputSampleIndex) {
            return ::media::Result<AudioSwrCompensationWindow>::failure(
                ::media::ErrorInfo::notInitialized(
                    "external audio correction requires a contiguous command"));
        }
        if (!swr) {
            return ::media::Result<AudioSwrCompensationWindow>::failure(
                ::media::ErrorInfo::notInitialized(
                    "audio correction requires initialized SwrContext"));
        }
        m_active = m_pending.front();
        m_pending.pop_front();
        const int result = swr_set_compensation(
            swr, m_active->sampleDelta(), m_active->compensationDistance());
        if (result < 0) {
            return ::media::Result<AudioSwrCompensationWindow>::failure(
                ::media::ErrorInfo::internalError(
                    "swr_set_compensation rejected planned audio correction"));
        }
        m_activeRemaining = m_active->compensationDistance();
    }
    return ::media::Result<AudioSwrCompensationWindow>::success(
        AudioSwrCompensationWindow{true, m_activeRemaining});
}

::media::Status AudioSwrCompensationExecutor::advance(int producedSamples)
{
    if (producedSamples < 0 ||
        (m_mode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired &&
         producedSamples > m_activeRemaining)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "audio correction executor advance exceeds active window"));
    }
    if (m_mode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        m_activeRemaining -= producedSamples;
        if (m_activeRemaining == 0 && m_active) {
            const auto delta = static_cast<std::int64_t>(m_active->sampleDelta());
            if ((delta > 0 &&
                 m_appliedNetSampleDelta >
                     std::numeric_limits<std::int64_t>::max() - delta) ||
                (delta < 0 &&
                 m_appliedNetSampleDelta <
                     -std::numeric_limits<std::int64_t>::max() - delta)) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "audio correction applied sample delta overflow"));
            }
            m_appliedNetSampleDelta += delta;
        }
    }
    return ::media::Status::success();
}

::media::Status AudioSwrCompensationExecutor::reset(std::uint64_t generation)
{
    if ((m_mode == MediaAudioCorrectionExecutionMode::Disabled && generation != 0) ||
        (m_mode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired &&
         generation == 0)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "audio correction reset generation violates execution mode"));
    }
    m_generation = generation;
    m_lastSequence = 0;
    m_lastPlannedEnd = 0;
    m_active.reset();
    m_pending.clear();
    m_activeRemaining = 0;
    m_appliedNetSampleDelta = 0;
    return ::media::Status::success();
}

bool AudioSwrCompensationExecutor::canAccept() const noexcept
{
    if (m_mode != MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        return false;
    }
    const std::size_t active = m_activeRemaining > 0 ? 1 : 0;
    return active + m_pending.size() < m_lookaheadWindows;
}

bool AudioSwrCompensationExecutor::requiresNextWindow() const noexcept
{
    return m_mode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired &&
           m_activeRemaining == 0;
}

::media::Status AudioSwrCompensationExecutor::settleTerminal()
{
    if (m_mode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired &&
        (m_activeRemaining != 0 || !m_pending.empty())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "audio correction terminated with unexecuted windows"));
    }
    m_active.reset();
    return ::media::Status::success();
}

::media::Status AudioSwrCompensationExecutor::settleTerminal(
    AudioSwrResamplerExhausted)
{
    m_active.reset();
    m_pending.clear();
    m_activeRemaining = 0;
    return ::media::Status::success();
}

MediaAudioCorrectionExecutionMode AudioSwrCompensationExecutor::mode() const noexcept
{
    return m_mode;
}

std::uint64_t AudioSwrCompensationExecutor::generation() const noexcept
{
    return m_generation;
}

std::int64_t AudioSwrCompensationExecutor::outstandingAuthorizedDroppedSamples() const noexcept
{
    return m_appliedNetSampleDelta < 0 ? -m_appliedNetSampleDelta : 0;
}

} // namespace media::ffmpeg::graph

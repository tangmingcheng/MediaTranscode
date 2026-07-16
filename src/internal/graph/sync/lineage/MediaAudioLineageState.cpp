#include "internal/graph/sync/lineage/MediaAudioLineageState.h"

namespace media::ffmpeg::graph {

MediaAudioLineageState::MediaAudioLineageState(
    bool synchronized,
    std::size_t capacity) noexcept
    : m_synchronized(synchronized)
    , m_capacity(capacity)
{
}

::media::Status MediaAudioLineageState::validateObservation(
    std::uint64_t generation) const
{
    std::lock_guard lock(m_mutex);
    if (generation == 0 || (m_generation != 0 && m_generation != generation)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio lineage state rejects an unpurged generation change"));
    }
    return ::media::Status::success();
}

::media::Status MediaAudioLineageState::observe(std::uint64_t generation)
{
    std::lock_guard lock(m_mutex);
    if (auto status = validateObservation(generation); !status) {
        return status;
    }
    m_generation = generation;
    return ::media::Status::success();
}

::media::Status MediaAudioLineageState::authorizeRetainedControl(
    const MediaBufferRef& buffer)
{
    std::lock_guard lock(m_mutex);
    if (!m_synchronized) {
        return ::media::Status::success();
    }
    return m_retainedControl.capture(
        buffer, m_generation, MediaStreamKind::Audio);
}

bool MediaAudioLineageState::retainedControlIsCurrent(
    const MediaBufferRef& buffer) const noexcept
{
    std::lock_guard lock(m_mutex);
    return !m_synchronized || m_retainedControl.matches(
        buffer, m_generation, MediaStreamKind::Audio);
}

bool MediaAudioLineageState::pendingOutputIsCurrent(
    const MediaBufferRef& buffer,
    std::optional<std::uint64_t> mediaGeneration) const noexcept
{
    std::lock_guard lock(m_mutex);
    if (!m_synchronized) {
        return true;
    }
    if (MediaRetainedControlFreshness::isCandidate(buffer)) {
        return m_retainedControl.matches(
            buffer, m_generation, MediaStreamKind::Audio);
    }
    return mediaGeneration.has_value() &&
           m_generation == *mediaGeneration;
}

void MediaAudioLineageState::resetRetainedControlFreshness() noexcept
{
    std::lock_guard lock(m_mutex);
    m_retainedControl.clear();
}

void MediaAudioLineageState::resetLifecycleLineage() noexcept
{
    std::lock_guard lock(m_mutex);
    m_generation = 0;
    m_lastTransitionSequence = 0;
    m_retainedControl.clear();
}

bool MediaAudioLineageState::isCurrent(std::uint64_t generation) const noexcept
{
    std::lock_guard lock(m_mutex);
    return m_generation == 0 || m_generation == generation;
}

bool MediaAudioLineageState::synchronized() const noexcept
{
    return m_synchronized;
}

std::size_t MediaAudioLineageState::capacity() const noexcept
{
    return m_capacity;
}

MediaAudioLineageState::Lock MediaAudioLineageState::lock() const
{
    return Lock(m_mutex);
}

::media::Status MediaAudioLineageState::purge(const MediaAvGenerationPurge& purge)
{
    std::lock_guard lock(m_mutex);
    if (purge.oldGeneration == 0 || purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence <= m_lastTransitionSequence ||
        (m_generation != 0 && m_generation != purge.oldGeneration)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio lineage purge requires exact current generation and fresh sequence"));
    }
    m_retainedControl.clear();
    clearOwnedLineage(purge);
    m_generation = purge.nextGeneration;
    m_lastTransitionSequence = purge.transitionSequence;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

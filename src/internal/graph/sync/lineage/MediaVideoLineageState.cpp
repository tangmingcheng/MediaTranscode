#include "internal/graph/sync/lineage/MediaVideoLineageState.h"

namespace media::ffmpeg::graph {

MediaVideoLineageState::MediaVideoLineageState(
    std::shared_ptr<MediaCodecLineageRegistry> registry) noexcept
    : m_registry(std::move(registry))
    , m_synchronized(static_cast<bool>(m_registry))
{
}

MediaVideoLineageState::MediaVideoLineageState(
    bool synchronized,
    std::shared_ptr<MediaCodecLineageRegistry> registry) noexcept
    : m_registry(std::move(registry))
    , m_synchronized(synchronized)
{
}

bool MediaVideoLineageState::synchronized() const noexcept
{
    return m_synchronized;
}

std::shared_ptr<MediaCodecLineageRegistry>
MediaVideoLineageState::registry() const noexcept
{
    return m_registry;
}

MediaVideoLineageState::Lock MediaVideoLineageState::lock() const
{
    return Lock(m_mutex);
}

::media::Status MediaVideoLineageState::observe(std::uint64_t generation)
{
    std::lock_guard lock(m_mutex);
    if (auto status = validateObservation(generation); !status) {
        return status;
    }
    m_generation = generation;
    return ::media::Status::success();
}

::media::Status MediaVideoLineageState::validateObservation(
    std::uint64_t generation) const
{
    std::lock_guard lock(m_mutex);
    if (!m_synchronized) {
        return ::media::Status::success();
    }
    if (generation == 0 || (m_generation != 0 && m_generation != generation)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video lineage state rejected an unpurged generation change"));
    }
    return ::media::Status::success();
}

::media::Status MediaVideoLineageState::authorizeRetainedControl(
    const MediaBufferRef& buffer)
{
    std::lock_guard lock(m_mutex);
    if (!m_synchronized) {
        return ::media::Status::success();
    }
    return m_retainedControl.capture(
        buffer, m_generation, MediaStreamKind::Video);
}

bool MediaVideoLineageState::pendingOutputIsCurrent(
    const MediaBufferRef& buffer,
    std::optional<std::uint64_t> mediaGeneration) const noexcept
{
    std::lock_guard lock(m_mutex);
    if (!m_synchronized) {
        return true;
    }
    if (MediaRetainedControlFreshness::isCandidate(buffer)) {
        return m_retainedControl.matches(
            buffer, m_generation, MediaStreamKind::Video);
    }
    return mediaGeneration && *mediaGeneration == m_generation;
}

void MediaVideoLineageState::resetGenerationLifecycle() noexcept
{
    std::lock_guard lock(m_mutex);
    m_retainedControl.clear();
    m_generation = 0;
    m_lastTransitionSequence = 0;
}

::media::Status MediaVideoLineageState::purge(
    const MediaAvGenerationPurge& purge)
{
    std::lock_guard lock(m_mutex);
    if (!m_synchronized || purge.oldGeneration == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence <= m_lastTransitionSequence ||
        (m_generation != 0 && m_generation != purge.oldGeneration)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video lineage purge requires exact current generation and fresh sequence"));
    }
    if (m_registry) {
        if (auto status = m_registry->purge(purge); !status) {
            return status;
        }
    }
    m_retainedControl.clear();
    clearOwnedLineage(purge);
    m_generation = purge.nextGeneration;
    m_lastTransitionSequence = purge.transitionSequence;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

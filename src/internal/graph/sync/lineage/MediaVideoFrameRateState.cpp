#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"

#include <algorithm>

namespace media::ffmpeg::graph {

MediaVideoFrameRateState::MediaVideoFrameRateState(
    bool requireCanonicalLineage) noexcept
    : m_requireCanonicalLineage(requireCanonicalLineage)
{
}

std::unique_lock<std::recursive_mutex> MediaVideoFrameRateState::lock() const
{
    return std::unique_lock<std::recursive_mutex>(m_mutex);
}

MediaVideoFrameRateState::Data& MediaVideoFrameRateState::data() noexcept
{
    return m_data;
}

const MediaVideoFrameRateState::Data& MediaVideoFrameRateState::data() const noexcept
{
    return m_data;
}

bool MediaVideoFrameRateState::requiresCanonicalLineage() const noexcept
{
    return m_requireCanonicalLineage;
}

void MediaVideoFrameRateState::resetTimelineLocked() noexcept
{
    m_data.initialized = false;
    m_data.started = false;
    m_data.flushed = false;
    m_data.inputTimeBase = {0, 1};
    m_data.targetFramePeriod = {0, 1};
    m_data.startPts = 0;
    m_data.nextOutputIndex = 0;
    m_data.lastInputPts = AV_NOPTS_VALUE;
    m_data.lastOutputPts = AV_NOPTS_VALUE;
}

void MediaVideoFrameRateState::resetLifecycle() noexcept
{
    auto guard = lock();
    resetTimelineLocked();
    m_data.lastInputFrame = {};
    m_data.pendingFrames.clear();
    m_data.activeGeneration = 0;
    m_data.expectedGeneration = 0;
    m_data.lastTransitionSequence = 0;
}

::media::Status MediaVideoFrameRateState::activateGeneration(
    std::uint64_t generation)
{
    auto guard = lock();
    if (!m_requireCanonicalLineage) {
        return ::media::Status::success();
    }
    if (generation == 0 ||
        (m_data.expectedGeneration != 0 &&
         generation != m_data.expectedGeneration) ||
        (m_data.activeGeneration != 0 &&
         generation != m_data.activeGeneration)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video frame-rate state rejected stale or skipped generation"));
    }
    if (m_data.activeGeneration == 0) {
        m_data.activeGeneration = generation;
        m_data.expectedGeneration = 0;
    }
    return ::media::Status::success();
}

::media::Status MediaVideoFrameRateState::purge(
    const MediaAvGenerationPurge& purge)
{
    if (purge.oldGeneration == 0 || purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video frame-rate purge requires a valid generation transition"));
    }

    auto guard = lock();
    if (purge.transitionSequence <= m_data.lastTransitionSequence ||
        (m_data.activeGeneration != 0 &&
         m_data.activeGeneration != purge.oldGeneration)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video frame-rate purge does not match active generation"));
    }

    std::erase_if(m_data.pendingFrames, [&](const TaggedFrame& pending) {
        return pending.generation == purge.oldGeneration;
    });
    if (m_data.lastInputFrame.generation == purge.oldGeneration) {
        m_data.lastInputFrame = {};
    }
    resetTimelineLocked();
    m_data.activeGeneration = 0;
    m_data.expectedGeneration = purge.nextGeneration;
    m_data.lastTransitionSequence = purge.transitionSequence;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

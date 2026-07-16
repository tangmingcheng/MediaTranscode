#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"

#include <algorithm>

namespace media::ffmpeg::graph {

MediaVideoFrameRateState::MediaVideoFrameRateState(
    bool requireCanonicalLineage) noexcept
    : MediaVideoLineageState(requireCanonicalLineage, nullptr)
    , m_requireCanonicalLineage(requireCanonicalLineage)
{
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
    clearLineageStorage();
    resetGenerationLifecycle();
}

void MediaVideoFrameRateState::clearLineageStorage() noexcept
{
    resetTimelineLocked();
    m_data.lastInputFrame = {};
    m_data.pendingFrames.clear();
    m_data.activeGeneration = 0;
    m_data.expectedGeneration = 0;
    m_data.terminals.reset();
    m_data.eofEmitted = false;
    m_data.terminalBuffer.reset();
    m_data.terminalPending = false;
    m_data.terminalIsEof = false;
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
    if (auto status = observe(generation); !status) {
        return status;
    }
    if (m_data.activeGeneration == 0) {
        m_data.activeGeneration = generation;
        m_data.expectedGeneration = 0;
    }
    return ::media::Status::success();
}

void MediaVideoFrameRateState::clearOwnedLineage(
    const MediaAvGenerationPurge& purge) noexcept
{
    clearLineageStorage();
    m_data.expectedGeneration = purge.nextGeneration;
}

} // namespace media::ffmpeg::graph

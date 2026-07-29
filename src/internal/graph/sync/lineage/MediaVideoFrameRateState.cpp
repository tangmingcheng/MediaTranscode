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

::media::Result<MediaVideoFrameRateGenerationDisposition>
MediaVideoFrameRateState::activateGeneration(
    std::uint64_t generation)
{
    auto guard = lock();
    if (!m_requireCanonicalLineage) {
        return ::media::Result<
            MediaVideoFrameRateGenerationDisposition>::success(
                MediaVideoFrameRateGenerationDisposition::Activate);
    }
    if (generation == 0) {
        return ::media::Result<
            MediaVideoFrameRateGenerationDisposition>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video frame-rate state requires a nonzero generation"));
    }
    const std::uint64_t requiredGeneration =
        m_data.expectedGeneration != 0
        ? m_data.expectedGeneration
        : m_data.activeGeneration;
    if (requiredGeneration != 0 && generation < requiredGeneration) {
        return ::media::Result<
            MediaVideoFrameRateGenerationDisposition>::success(
                MediaVideoFrameRateGenerationDisposition::DropStale);
    }
    if (requiredGeneration != 0 && generation > requiredGeneration) {
        return ::media::Result<
            MediaVideoFrameRateGenerationDisposition>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video frame-rate state rejected a skipped generation"));
    }
    if (auto status = observe(generation); !status) {
        return ::media::Result<
            MediaVideoFrameRateGenerationDisposition>::failure(
                status.error());
    }
    if (m_data.activeGeneration == 0) {
        m_data.activeGeneration = generation;
        m_data.expectedGeneration = 0;
    }
    return ::media::Result<
        MediaVideoFrameRateGenerationDisposition>::success(
            MediaVideoFrameRateGenerationDisposition::Activate);
}

void MediaVideoFrameRateState::clearOwnedLineage(
    const MediaAvGenerationPurge& purge) noexcept
{
    clearLineageStorage();
    m_data.expectedGeneration = purge.nextGeneration;
}

} // namespace media::ffmpeg::graph

#include "internal/graph/runtime/profiling/MediaGraphProfiler.h"

namespace media::ffmpeg::graph {

void MediaGraphProfiler::record(MediaNodeId nodeId, int64_t elapsedUs, bool success) noexcept
{
    if (!nodeId) {
        return;
    }

    auto& stats = m_nodeStats[nodeId.value];
    ++stats.calls;
    if (!success) {
        ++stats.errors;
    }
    stats.totalUs += elapsedUs;
    if (elapsedUs > stats.maxUs) {
        stats.maxUs = elapsedUs;
    }
}

const std::unordered_map<uint32_t, MediaNodeProfileStats>& MediaGraphProfiler::nodeStats() const noexcept
{
    return m_nodeStats;
}

void MediaGraphProfiler::clear()
{
    m_nodeStats.clear();
}

MediaNodeProfileScope::MediaNodeProfileScope(MediaGraphProfiler& profiler, MediaNodeId nodeId) noexcept
    : m_profiler(profiler)
    , m_nodeId(nodeId)
    , m_start(MediaGraphProfiler::Clock::now())
{
}

MediaNodeProfileScope::~MediaNodeProfileScope()
{
    const auto end = MediaGraphProfiler::Clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
    m_profiler.record(m_nodeId, elapsed, m_success);
}

void MediaNodeProfileScope::setSuccess(bool success) noexcept
{
    m_success = success;
}

} // namespace media::ffmpeg::graph

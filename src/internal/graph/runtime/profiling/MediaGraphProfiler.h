#pragma once

#include "internal/graph/core/MediaNodeId.h"

#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace media::ffmpeg::graph {

struct MediaNodeProfileStats {
    uint64_t calls = 0;
    uint64_t errors = 0;
    int64_t totalUs = 0;
    int64_t maxUs = 0;

    int64_t averageUs() const noexcept
    {
        return calls == 0 ? 0 : totalUs / static_cast<int64_t>(calls);
    }
};

class MediaGraphProfiler final {
public:
    using Clock = std::chrono::steady_clock;

    void record(MediaNodeId nodeId, int64_t elapsedUs, bool success) noexcept;
    const std::unordered_map<uint32_t, MediaNodeProfileStats>& nodeStats() const noexcept;
    void clear();

private:
    std::unordered_map<uint32_t, MediaNodeProfileStats> m_nodeStats;
};

class MediaNodeProfileScope final {
public:
    MediaNodeProfileScope(MediaGraphProfiler& profiler, MediaNodeId nodeId) noexcept;
    ~MediaNodeProfileScope();

    MediaNodeProfileScope(const MediaNodeProfileScope&) = delete;
    MediaNodeProfileScope& operator=(const MediaNodeProfileScope&) = delete;

    void setSuccess(bool success) noexcept;

private:
    MediaGraphProfiler& m_profiler;
    MediaNodeId m_nodeId;
    MediaGraphProfiler::Clock::time_point m_start;
    bool m_success = true;
};

} // namespace media::ffmpeg::graph

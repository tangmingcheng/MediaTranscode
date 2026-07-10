#pragma once

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaThreadingMode {
    SingleThreaded,
    PerNodeWorker,
    WorkerPool,
    Hybrid
};

enum class MediaThreadPriority {
    Normal,
    High,
    Realtime
};

struct MediaThreadingPolicy {
    MediaThreadingMode mode = MediaThreadingMode::SingleThreaded;
    MediaThreadPriority priority = MediaThreadPriority::Normal;

    std::size_t maxWorkerThreads = 0;
    bool pinWorkers = false;
    bool collectWorkerMetrics = true;

    constexpr bool threaded() const noexcept
    {
        return mode != MediaThreadingMode::SingleThreaded;
    }
};

} // namespace media::ffmpeg::graph

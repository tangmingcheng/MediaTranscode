#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace media::ffmpeg::graph {

struct MediaGraphWorkerConfig {
    uint32_t idleSleepMs = 1;
    uint32_t maxIdleSpins = 4;
    uint32_t maxConsecutiveErrors = 1;
};

struct MediaGraphWorkerMetrics {
    uint64_t iterations = 0;
    uint64_t idleIterations = 0;
    uint64_t errors = 0;
};

class MediaGraphWorker final {
public:
    MediaGraphWorker(MediaRuntimeNode& node,
                     MediaGraphExecutionContext& context,
                     MediaGraphWorkerConfig config = {});
    ~MediaGraphWorker();

    MediaGraphWorker(const MediaGraphWorker&) = delete;
    MediaGraphWorker& operator=(const MediaGraphWorker&) = delete;

    ::media::Status start();
    void requestStop() noexcept;
    void abort() noexcept;
    void join();

    bool running() const noexcept;
    bool stopRequested() const noexcept;
    bool aborted() const noexcept;

    MediaNodeId nodeId() const noexcept;
    const MediaGraphWorkerMetrics& metrics() const noexcept;

private:
    void run();

private:
    MediaRuntimeNode& m_node;
    MediaGraphExecutionContext& m_context;
    MediaGraphWorkerConfig m_config;
    std::thread m_thread;
    std::atomic_bool m_running{ false };
    std::atomic_bool m_stopRequested{ false };
    std::atomic_bool m_aborted{ false };
    MediaGraphWorkerMetrics m_metrics;
};

} // namespace media::ffmpeg::graph

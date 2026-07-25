#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/threading/MediaGraphWorkerFailure.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace media::ffmpeg::graph {

struct MediaGraphWorkerConfig {
    uint32_t maxConsecutiveErrors = 1;
};

struct MediaGraphWorkerMetrics {
    std::atomic_uint64_t processCalls{ 0 };
    std::atomic_uint64_t progress{ 0 };
    std::atomic_uint64_t waits{ 0 };
    std::atomic_uint64_t wakeups{ 0 };
    std::atomic_uint64_t deadlines{ 0 };
    std::atomic_uint64_t errors{ 0 };
};

class MediaGraphWorker final {
public:
    MediaGraphWorker(MediaRuntimeNode& node,
                     MediaGraphExecutionContext& context,
                     MediaGraphWorkerConfig config = {});
    MediaGraphWorker(MediaRuntimeNode& node,
                     MediaGraphExecutionContext& context,
                     MediaGraphWorkerFailureRecorder& failureRecorder,
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
    void recordFailure(::media::ErrorInfo error);
    void run();

private:
    MediaRuntimeNode& m_node;
    MediaGraphExecutionContext& m_context;
    MediaNodeWakeup& m_wakeup;
    MediaGraphWorkerFailureRecorder m_localFailureRecorder;
    MediaGraphWorkerFailureRecorder* m_failureRecorder = nullptr;
    MediaGraphWorkerConfig m_config;
    std::thread m_thread;
    std::atomic_bool m_running{ false };
    std::atomic_bool m_stopRequested{ false };
    std::atomic_bool m_aborted{ false };
    MediaGraphWorkerMetrics m_metrics;
};

} // namespace media::ffmpeg::graph

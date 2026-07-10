#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <memory>

namespace media::ffmpeg::graph {

enum class MediaGraphRuntimeState {
    Empty,
    Compiled,
    Running,
    ThreadedRunning,
    Stopped,
    Aborted
};

struct MediaGraphRunResult {
    std::size_t iterations = 0;
    std::size_t idleIterations = 0;
    std::uint64_t totalPushed = 0;
    std::uint64_t totalPopped = 0;
    std::uint64_t totalClosed = 0;
    std::uint64_t totalAborted = 0;
    std::uint64_t totalCleared = 0;
    std::size_t queuedBuffers = 0;
    bool completed = false;
};

class MediaGraphRuntime final {
public:
    MediaGraphRuntime() = default;

    MediaGraphRuntime(const MediaGraphRuntime&) = delete;
    MediaGraphRuntime& operator=(const MediaGraphRuntime&) = delete;

    void setDiagnosticsEnabled(bool enabled) noexcept;
    bool diagnosticsEnabled() const noexcept;

    ::media::Status compile(MediaGraph graph);
    ::media::Status registerRuntimeNode(std::unique_ptr<MediaRuntimeNode> node);
    ::media::Status registerDefaultRuntimeNodes();

    void setThreadingPolicy(MediaThreadingPolicy policy) noexcept;
    const MediaThreadingPolicy& threadingPolicy() const noexcept;

    ::media::Result<MediaGraphRunResult> run();
    ::media::Status startThreaded();
    ::media::Status synchronizeThreadedState();
    ::media::Status flush();
    ::media::Status stop();
    void abort() noexcept;
    void reset();

    MediaGraphRuntimeState state() const noexcept;
    bool compiled() const noexcept;
    bool running() const noexcept;
    bool threadedRunning() const noexcept;

    MediaGraphExecutionContext& context() noexcept;
    const MediaGraphExecutionContext& context() const noexcept;

    MediaGraphScheduler& scheduler() noexcept;
    const MediaGraphScheduler& scheduler() const noexcept;

    MediaGraphThreadedExecutor& threadedExecutor() noexcept;
    const MediaGraphThreadedExecutor& threadedExecutor() const noexcept;

    const MediaGraph* graph() const noexcept;
    MediaRuntimeAcceptanceCollector& acceptanceCollector() noexcept;
    const MediaRuntimeAcceptanceCollector& acceptanceCollector() const noexcept;
    std::size_t observeQueueHighWatermark(std::size_t queued) const noexcept;

private:
    MediaGraph m_graph;
    MediaGraphExecutionContext m_context;
    MediaGraphScheduler m_scheduler;
    MediaGraphThreadedExecutor m_threadedExecutor;
    MediaThreadingPolicy m_threadingPolicy;
    MediaGraphRuntimeState m_state = MediaGraphRuntimeState::Empty;
    MediaRuntimeAcceptanceCollector m_acceptanceCollector;
    mutable std::atomic_size_t m_queueHighWatermark{ 0 };
};

} // namespace media::ffmpeg::graph

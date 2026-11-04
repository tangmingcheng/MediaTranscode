#pragma once

#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "media_transcode/Result.h"

#include <memory>
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphThreadedExecutorState {
    Idle,
    Running,
    Stopping,
    Stopped,
    Aborted,
    Starting
};

struct MediaGraphNodeCompletion final {
    bool finished;
    bool exited;
};

class MediaGraphThreadedExecutor final {
public:
    MediaGraphThreadedExecutor() = default;

    MediaGraphThreadedExecutor(const MediaGraphThreadedExecutor&) = delete;
    MediaGraphThreadedExecutor& operator=(const MediaGraphThreadedExecutor&) = delete;

    void setPolicy(MediaThreadingPolicy policy) noexcept;
    const MediaThreadingPolicy& policy() const noexcept;

    ::media::Status start(MediaGraphExecutionContext& context,
                          MediaGraphScheduler& scheduler);
    // Interrupt execution while retaining nodes, channels and metadata owners.
    // The existing stop/abort operation performs the later join and teardown.
    void requestStop(MediaGraphExecutionContext& context) noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context,
                         MediaGraphScheduler& scheduler);
    void abort(MediaGraphExecutionContext& context,
               MediaGraphScheduler& scheduler) noexcept;
    void clear();

    MediaGraphThreadedExecutorState state() const noexcept;
    bool running() const noexcept;
    bool completed() const noexcept;
    bool failed() const noexcept;
    std::optional<MediaGraphWorkerFailure> primaryFailure() const;
    MediaGraphRuntimeMetrics metrics() const noexcept;
    ::media::Result<MediaGraphNodeCompletion> nodeCompletion(MediaNodeId node) const;

private:
    void refreshMetrics() const noexcept;

private:
    MediaThreadingPolicy m_policy;
    std::atomic<MediaGraphThreadedExecutorState> m_state{MediaGraphThreadedExecutorState::Idle};
    mutable std::recursive_mutex m_lifecycleMutex;
    mutable std::mutex m_workersMutex;
    MediaWorkerStartupGate m_startup;
    MediaGraphWorkerFailureRecorder m_failureRecorder;
    MediaGraphWorkerFailureSupervisor m_failureSupervisor;
    MediaFixedCapacityStorage<std::unique_ptr<MediaGraphWorker>> m_workers;
    mutable MediaGraphRuntimeMetrics m_metrics;
    mutable std::mutex m_metricsMutex;
};

} // namespace media::ffmpeg::graph

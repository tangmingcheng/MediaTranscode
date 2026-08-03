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
    Starting,
    Running,
    Stopping,
    Stopped,
    Aborted
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

private:
    void refreshMetrics() const noexcept;

private:
    MediaThreadingPolicy m_policy;
    MediaGraphThreadedExecutorState m_state = MediaGraphThreadedExecutorState::Idle;
    std::vector<std::unique_ptr<MediaGraphWorker>> m_workers;
    MediaGraphWorkerFailureRecorder m_failureRecorder;
    MediaGraphWorkerFailureSupervisor m_failureSupervisor;
    mutable MediaGraphRuntimeMetrics m_metrics;
    mutable std::mutex m_metricsMutex;
};

} // namespace media::ffmpeg::graph

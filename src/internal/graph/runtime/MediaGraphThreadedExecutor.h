#pragma once

#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaGraphRuntimeMetrics.h"
#include "internal/graph/runtime/MediaGraphScheduler.h"
#include "internal/graph/runtime/MediaGraphWorker.h"
#include "media_transcode/Result.h"

#include <memory>
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
    const MediaGraphRuntimeMetrics& metrics() const noexcept;

private:
    void refreshMetrics() noexcept;

private:
    MediaThreadingPolicy m_policy;
    MediaGraphThreadedExecutorState m_state = MediaGraphThreadedExecutorState::Idle;
    std::vector<std::unique_ptr<MediaGraphWorker>> m_workers;
    MediaGraphRuntimeMetrics m_metrics;
};

} // namespace media::ffmpeg::graph

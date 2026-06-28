#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

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

class MediaGraphRuntime final {
public:
    MediaGraphRuntime() = default;

    MediaGraphRuntime(const MediaGraphRuntime&) = delete;
    MediaGraphRuntime& operator=(const MediaGraphRuntime&) = delete;

    ::media::Status compile(MediaGraph graph);
    ::media::Status registerRuntimeNode(std::unique_ptr<MediaRuntimeNode> node);
    ::media::Status registerDefaultRuntimeNodes();

    void setThreadingPolicy(MediaThreadingPolicy policy) noexcept;
    const MediaThreadingPolicy& threadingPolicy() const noexcept;

    ::media::Status start();
    ::media::Status startThreaded();
    ::media::Status processOnce();
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

private:
    MediaGraph m_graph;
    MediaGraphExecutionContext m_context;
    MediaGraphScheduler m_scheduler;
    MediaGraphThreadedExecutor m_threadedExecutor;
    MediaThreadingPolicy m_threadingPolicy;
    MediaGraphRuntimeState m_state = MediaGraphRuntimeState::Empty;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaGraphScheduler.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

enum class MediaGraphRuntimeState {
    Empty,
    Compiled,
    Running,
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

    ::media::Status start();
    ::media::Status processOnce();
    ::media::Status flush();
    ::media::Status stop();
    void abort() noexcept;
    void reset();

    MediaGraphRuntimeState state() const noexcept;
    bool compiled() const noexcept;
    bool running() const noexcept;

    MediaGraphExecutionContext& context() noexcept;
    const MediaGraphExecutionContext& context() const noexcept;

    MediaGraphScheduler& scheduler() noexcept;
    const MediaGraphScheduler& scheduler() const noexcept;

    const MediaGraph* graph() const noexcept;

private:
    MediaGraph m_graph;
    MediaGraphExecutionContext m_context;
    MediaGraphScheduler m_scheduler;
    MediaGraphRuntimeState m_state = MediaGraphRuntimeState::Empty;
};

} // namespace media::ffmpeg::graph

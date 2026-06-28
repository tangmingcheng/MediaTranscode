#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/execution/MediaGraphExecutionOptions.h"
#include "internal/graph/runtime/execution/MediaGraphExecutionResult.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

enum class MediaGraphExecutionEngineState {
    Empty,
    Prepared,
    Running,
    Completed,
    Stopped,
    Failed
};

class MediaGraphExecutionEngine final {
public:
    ::media::Status prepare(MediaGraph graph,
                            MediaGraphExecutionOptions options = {});

    ::media::Status start();
    ::media::Status processOnce();
    ::media::Result<MediaGraphExecutionResult> runUntilIdle();
    ::media::Status stop();
    void abort() noexcept;
    void reset();

    MediaGraphRuntime& runtime() noexcept;
    const MediaGraphRuntime& runtime() const noexcept;

    MediaGraphRuntimeReport report() const;
    MediaGraphExecutionEngineState state() const noexcept;
    const MediaGraphExecutionOptions& options() const noexcept;

    static ::media::Result<MediaGraphExecutionResult> execute(
        MediaGraph graph,
        MediaGraphExecutionOptions options = {});

private:
    MediaGraphRuntime m_runtime;
    MediaGraphExecutionOptions m_options;
    MediaGraphExecutionEngineState m_state = MediaGraphExecutionEngineState::Empty;
};

} // namespace media::ffmpeg::graph

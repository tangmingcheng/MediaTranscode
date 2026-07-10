#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <memory>

namespace media::ffmpeg::graph {

class PacketMergeNode final : public FFmpegNodeRuntime {
public:
    explicit PacketMergeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    void bindInputs(MediaGraphExecutionContext& context);
    void resetRuntimeState() noexcept;

    std::unique_ptr<MediaInputTerminalTracker> m_terminals;
    MediaBufferRef m_terminalBuffer;
};

} // namespace media::ffmpeg::graph

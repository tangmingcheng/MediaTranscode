#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

#include <atomic>

struct AVFormatContext;

namespace media::ffmpeg::graph {

class DemuxNode final : public FFmpegNodeRuntime {
public:
    explicit DemuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    bool abortRequested() const noexcept;
    bool hasBoundFormatContext() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;

private:
    void resetRuntimeState() noexcept;
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status emitEof(MediaGraphExecutionContext& context);

private:
    ::media::ffmpeg::InputFormatContextPtr m_formatContextOwner;
    AVFormatContext* m_formatContext = nullptr;
    std::atomic_bool m_abortRequested { false };
    bool m_eofSent = false;
};

} // namespace media::ffmpeg::graph

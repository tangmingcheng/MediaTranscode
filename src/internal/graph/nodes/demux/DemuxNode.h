#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <atomic>

struct AVFormatContext;

namespace media::ffmpeg::graph {

class DemuxNode final : public FFmpegNodeRuntime {
public:
    explicit DemuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    bool abortRequested() const noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status emitEof(MediaGraphExecutionContext& context);

private:
    MediaBufferRef m_formatContextOwner;
    AVFormatContext* m_formatContext = nullptr;
    std::atomic_bool m_abortRequested { false };
    bool m_eofSent = false;
};

} // namespace media::ffmpeg::graph

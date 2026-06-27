#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class DemuxNode final : public FFmpegNodeRuntime {
public:
    explicit DemuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status emitEof(MediaGraphExecutionContext& context);

private:
    MediaBufferRef m_formatContextOwner;
    AVFormatContext* m_formatContext = nullptr;
    bool m_eofSent = false;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class FileOutputNode final : public FFmpegNodeRuntime {
public:
    explicit FileOutputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Result<MediaBufferRef> createResource(
        MediaGraphExecutionContext& context);

private:
    bool m_emitted = false;
    MediaBufferRef m_resource;
};

} // namespace media::ffmpeg::graph

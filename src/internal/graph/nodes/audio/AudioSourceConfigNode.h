#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

struct AVFormatContext;

namespace media::ffmpeg::graph {

class AudioSourceConfigNode final : public FFmpegNodeRuntime {
public:
    explicit AudioSourceConfigNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status bindSourceStreamIndex(MediaGraphExecutionContext& context);
    ::media::Status emitSourceConfig(MediaGraphExecutionContext& context);

private:
    MediaBufferRef m_formatContextOwner;
    AVFormatContext* m_formatContext = nullptr;
    int m_sourceStreamIndex = invalidMediaStreamIndex;
    bool m_emitted = false;
};

} // namespace media::ffmpeg::graph

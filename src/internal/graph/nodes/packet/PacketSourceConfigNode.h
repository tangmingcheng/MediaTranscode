#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

struct AVFormatContext;

namespace media::ffmpeg::graph {

class PacketSourceConfigNode final : public FFmpegNodeRuntime {
public:
    explicit PacketSourceConfigNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status bindSourceStream(MediaGraphExecutionContext& context);
    ::media::Status emitSourceConfig(MediaGraphExecutionContext& context);

private:
    MediaBufferRef m_formatContextOwner;
    AVFormatContext* m_formatContext = nullptr;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    int m_sourceStreamIndex = invalidMediaStreamIndex;
    bool m_emitted = false;
};

} // namespace media::ffmpeg::graph

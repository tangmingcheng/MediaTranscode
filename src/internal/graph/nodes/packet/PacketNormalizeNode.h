#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

struct AVFormatContext;

namespace media::ffmpeg::graph {

class PacketNormalizeNode final : public FFmpegNodeRuntime {
public:
    explicit PacketNormalizeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status bindSourceStream(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> normalizePacket(const MediaBufferRef& buffer);

private:
    MediaBufferRef m_formatContextOwner;
    AVFormatContext* m_formatContext = nullptr;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    int m_sourceStreamIndex = invalidMediaStreamIndex;
};

} // namespace media::ffmpeg::graph

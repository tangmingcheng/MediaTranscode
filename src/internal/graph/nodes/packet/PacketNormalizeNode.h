#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot;

class PacketNormalizeNode final : public FFmpegNodeRuntime {
public:
    explicit PacketNormalizeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    void releaseFormatContext() noexcept;
    ::media::Status bindFormatContext(MediaGraphExecutionContext& context);
    ::media::Status bindSourceStream(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> normalizePacket(const MediaBufferRef& buffer);
    ::media::Status normalizePacketTimestamps(MediaBufferRef& buffer);

private:
    MediaBufferRef m_formatContextOwner;
    const FFmpegInputStreamSnapshot* m_sourceStream = nullptr;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    int m_sourceStreamIndex = invalidMediaStreamIndex;
    bool m_monotonicPacketTimestamps = false;
    int64_t m_nextPacketDts = invalidMediaTimeValue;
};

} // namespace media::ffmpeg::graph

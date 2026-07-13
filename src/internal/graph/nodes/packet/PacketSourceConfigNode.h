#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot;

class PacketSourceConfigNode final : public FFmpegNodeRuntime {
public:
    explicit PacketSourceConfigNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    void releaseInputSnapshots() noexcept;
    ::media::Status bindInputSnapshots(MediaGraphExecutionContext& context);
    ::media::Status bindSourceStream(MediaGraphExecutionContext& context);
    ::media::Status emitSourceConfig(MediaGraphExecutionContext& context);

private:
    MediaBufferRef m_inputSnapshotOwner;
    const FFmpegInputStreamSnapshot* m_sourceStream = nullptr;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    int m_sourceStreamIndex = invalidMediaStreamIndex;
    bool m_emitted = false;
};

} // namespace media::ffmpeg::graph

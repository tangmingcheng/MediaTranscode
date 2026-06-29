#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg::graph {

class FileMuxNode final : public FFmpegNodeRuntime {
public:
    explicit FileMuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    bool tryBindOutputContext(const MediaBufferRef& buffer) noexcept;
    ::media::Status tryBindCodecContext(const MediaBufferRef& buffer);
    ::media::Status registerPendingCodecContexts();
    ::media::Status registerStreamFromCodecContext(const MediaBufferRef& buffer);
    ::media::Status writeHeaderIfNeeded();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writeTrailerIfNeeded();
    void releaseRuntimeViews() noexcept;
    ::media::Status forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);

private:
    AVFormatContext* m_outputContext = nullptr;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    int m_videoStreamIndex = invalidMediaStreamIndex;
    int m_audioStreamIndex = invalidMediaStreamIndex;
    std::vector<MediaBufferRef> m_pendingCodecContexts;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

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
    ::media::Status writeHeaderIfNeeded();
    ::media::Status writePacket(const MediaBufferRef& buffer);
    ::media::Status writeTrailerIfNeeded();
    ::media::Status forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);

private:
    MediaBufferRef m_outputContextOwner;
    AVFormatContext* m_outputContext = nullptr;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
};

} // namespace media::ffmpeg::graph

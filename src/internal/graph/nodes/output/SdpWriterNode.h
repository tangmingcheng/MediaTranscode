#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class SdpWriterNode final : public FFmpegNodeRuntime {
public:
    explicit SdpWriterNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status writeSdp(MediaGraphExecutionContext& context, const std::string& path);

private:
    std::vector<MediaBufferRef> m_contextBuffers;
    bool m_written = false;
};

} // namespace media::ffmpeg::graph

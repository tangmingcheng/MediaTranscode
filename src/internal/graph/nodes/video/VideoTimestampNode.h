#pragma once

#include "internal/graph/model/MediaTimeDescriptor.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"

extern "C" {
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

class VideoTimestampNode final : public FFmpegNodeRuntime {
public:
    explicit VideoTimestampNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Status onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status bindCodecConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status normalizeFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);

private:
    bool m_hasTargetTimeBase = false;
    AVRational m_targetTimeBase { 0, 1 };
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class MetadataProbeNode final : public FFmpegNodeRuntime {
public:
    explicit MetadataProbeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph

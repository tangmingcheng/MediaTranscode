#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"

namespace media::ffmpeg::graph {

class MediaPlaybackEpochActivatedFanoutNode final : public FFmpegNodeRuntime {
public:
    explicit MediaPlaybackEpochActivatedFanoutNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
};

} // namespace media::ffmpeg::graph

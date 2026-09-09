#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/channel/MediaOutputBranchFanout.h"

namespace media::ffmpeg::graph {
class EncodedVideoOutputFanoutNode final : public FFmpegNodeRuntime {
public:
    explicit EncodedVideoOutputFanoutNode(MediaNodeId nodeId);
    ::media::Status subscribe(std::shared_ptr<MediaRuntimeBranch> branch, MediaEdgeId edge);
    void unsubscribe(std::uint64_t outputId);
protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
private:
    MediaOutputBranchFanout m_fanout;
};
} // namespace media::ffmpeg::graph

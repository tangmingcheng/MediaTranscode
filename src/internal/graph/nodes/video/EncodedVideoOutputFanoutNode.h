#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/channel/MediaOutputBranchFanout.h"
#include "internal/graph/model/MediaVideoJoinPlan.h"
#include <optional>

namespace media::ffmpeg::graph {
class EncodedVideoOutputFanoutNode final : public FFmpegNodeRuntime {
public:
    explicit EncodedVideoOutputFanoutNode(MediaNodeId nodeId);
    ::media::Status bindJoinPlan(MediaVideoJoinPlan plan);
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status subscribe(std::shared_ptr<MediaRuntimeBranch> branch, MediaEdgeId edge);
    void unsubscribe(std::uint64_t outputId);
protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
private:
    MediaOutputBranchFanout m_fanout;
    std::optional<MediaVideoJoinPlan> m_joinPlan;
    bool m_started = false;
};
} // namespace media::ffmpeg::graph

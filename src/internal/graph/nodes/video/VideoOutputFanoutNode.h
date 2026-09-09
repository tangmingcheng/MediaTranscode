#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/model/MediaVideoSourceEpochPlan.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/channel/MediaOutputBranchFanout.h"

#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

class VideoOutputFanoutNode final : public FFmpegNodeRuntime {
public:
    explicit VideoOutputFanoutNode(MediaNodeId nodeId);
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status subscribe(
        std::shared_ptr<MediaRuntimeBranch> branch, MediaEdgeId edge);
    void unsubscribe(std::uint64_t outputId);
    ::media::Result<::media::ffmpeg::BufferRefPtr> hardwareFrames() const;
    ::media::Result<std::uint64_t> sourceGeneration() const;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    MediaOutputBranchFanout m_fanout;
    mutable std::mutex m_factsMutex;
    ::media::ffmpeg::BufferRefPtr m_hardwareFrames;
    std::optional<std::uint64_t> m_generation;
    std::optional<MediaVideoSourceEpochPlan> m_epochPlan;
    bool m_frameFactsReady = false;
    bool m_hasObservedFrame = false;
    bool m_sourceContractInvalidated = false;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
    int m_frameFormat = -1;
};

} // namespace media::ffmpeg::graph

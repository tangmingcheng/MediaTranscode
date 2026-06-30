#pragma once

#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphSchedulerState {
    Idle,
    Configured,
    Running,
    Stopping,
    Stopped,
    Aborted
};

class MediaGraphScheduler final {
public:
    MediaGraphScheduler() = default;

    MediaGraphScheduler(const MediaGraphScheduler&) = delete;
    MediaGraphScheduler& operator=(const MediaGraphScheduler&) = delete;

    ::media::Status registerNode(std::unique_ptr<MediaRuntimeNode> node);
    MediaRuntimeNode* findNode(MediaNodeId nodeId);
    const MediaRuntimeNode* findNode(MediaNodeId nodeId) const;

    std::vector<MediaRuntimeNode*> orderedRuntimeNodes(const MediaGraphExecutionContext& context);
    std::vector<const MediaRuntimeNode*> orderedRuntimeNodes(const MediaGraphExecutionContext& context) const;

    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status start(MediaGraphExecutionContext& context);
    ::media::Status processSchedulingStep(MediaGraphExecutionContext& context);
    ::media::Status flush(MediaGraphExecutionContext& context);
    ::media::Status stop(MediaGraphExecutionContext& context);
    void abort(MediaGraphExecutionContext& context) noexcept;
    void clear();

    MediaGraphSchedulerState state() const noexcept;
    bool running() const noexcept;

private:
    std::unordered_map<uint32_t, std::unique_ptr<MediaRuntimeNode>> m_nodes;
    MediaGraphSchedulerState m_state = MediaGraphSchedulerState::Idle;
};

} // namespace media::ffmpeg::graph

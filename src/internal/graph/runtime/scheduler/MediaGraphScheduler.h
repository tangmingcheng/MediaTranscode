#pragma once

#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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

struct MediaGraphSchedulerNodeIdHash final {
    std::size_t operator()(std::uint32_t value) const noexcept
    {
        return static_cast<std::size_t>(value);
    }
};

struct MediaGraphSchedulerNodeIdEqual final {
    bool operator()(std::uint32_t left, std::uint32_t right) const noexcept
    {
        return left == right;
    }
};

class MediaGraphScheduler final {
public:
    MediaGraphScheduler() = default;

    MediaGraphScheduler(const MediaGraphScheduler&) = delete;
    MediaGraphScheduler& operator=(const MediaGraphScheduler&) = delete;

    ::media::Status registerNode(std::unique_ptr<MediaRuntimeNode> node);
    ::media::Status registerNodes(
        std::vector<std::unique_ptr<MediaRuntimeNode>> nodes);
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
    void clear(const MediaGraphExecutionContext* context = nullptr);
    void clear(const std::vector<MediaNodeId>& executionOrder);

    MediaGraphSchedulerState state() const noexcept;
    bool running() const noexcept;

private:
    std::unordered_map<std::uint32_t,
                       std::unique_ptr<MediaRuntimeNode>,
                       MediaGraphSchedulerNodeIdHash,
                       MediaGraphSchedulerNodeIdEqual> m_nodes;
    std::unordered_set<std::uint32_t,
                       MediaGraphSchedulerNodeIdHash,
                       MediaGraphSchedulerNodeIdEqual> m_finishedNodes;
    MediaGraphSchedulerState m_state = MediaGraphSchedulerState::Idle;
};

} // namespace media::ffmpeg::graph

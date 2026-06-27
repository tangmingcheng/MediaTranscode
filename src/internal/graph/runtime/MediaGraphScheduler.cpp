#include "internal/graph/runtime/MediaGraphScheduler.h"

#include <algorithm>

namespace media::ffmpeg::graph {

::media::Status MediaGraphScheduler::registerNode(std::unique_ptr<MediaRuntimeNode> node)
{
    if (!node || !node->nodeId()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphScheduler registerNode failed: node is invalid"));
    }

    const uint32_t key = node->nodeId().value;
    if (m_nodes.find(key) != m_nodes.end()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphScheduler registerNode failed: duplicate runtime node"));
    }

    m_nodes[key] = std::move(node);
    return ::media::Status::success();
}

MediaRuntimeNode* MediaGraphScheduler::findNode(MediaNodeId nodeId)
{
    const auto it = m_nodes.find(nodeId.value);
    return it == m_nodes.end() ? nullptr : it->second.get();
}

const MediaRuntimeNode* MediaGraphScheduler::findNode(MediaNodeId nodeId) const
{
    const auto it = m_nodes.find(nodeId.value);
    return it == m_nodes.end() ? nullptr : it->second.get();
}

::media::Status MediaGraphScheduler::configure(MediaGraphExecutionContext& context)
{
    if (!context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphScheduler configure failed: context is not compiled"));
    }

    for (MediaRuntimeNode* node : orderedNodes(context)) {
        auto status = node->configure(context);
        if (!status) {
            return status;
        }
    }

    m_state = MediaGraphSchedulerState::Configured;
    return ::media::Status::success();
}

::media::Status MediaGraphScheduler::start(MediaGraphExecutionContext& context)
{
    if (!context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphScheduler start failed: context is not compiled"));
    }

    if (m_state == MediaGraphSchedulerState::Idle) {
        auto status = configure(context);
        if (!status) {
            return status;
        }
    }

    for (MediaRuntimeNode* node : orderedNodes(context)) {
        auto status = node->start(context);
        if (!status) {
            return status;
        }
    }

    m_state = MediaGraphSchedulerState::Running;
    return ::media::Status::success();
}

::media::Status MediaGraphScheduler::processOnce(MediaGraphExecutionContext& context)
{
    if (m_state != MediaGraphSchedulerState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphScheduler processOnce failed: scheduler is not running"));
    }

    for (MediaRuntimeNode* node : orderedNodes(context)) {
        auto status = node->process(context);
        if (!status) {
            return status;
        }
    }

    return ::media::Status::success();
}

::media::Status MediaGraphScheduler::flush(MediaGraphExecutionContext& context)
{
    auto ordered = orderedNodes(context);
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
        auto status = (*it)->flush(context);
        if (!status) {
            return status;
        }
    }

    return ::media::Status::success();
}

::media::Status MediaGraphScheduler::stop(MediaGraphExecutionContext& context)
{
    if (m_state == MediaGraphSchedulerState::Stopped || m_state == MediaGraphSchedulerState::Idle) {
        m_state = MediaGraphSchedulerState::Stopped;
        return ::media::Status::success();
    }

    m_state = MediaGraphSchedulerState::Stopping;

    auto ordered = orderedNodes(context);
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
        auto status = (*it)->stop(context);
        if (!status) {
            return status;
        }
    }

    m_state = MediaGraphSchedulerState::Stopped;
    return ::media::Status::success();
}

void MediaGraphScheduler::abort(MediaGraphExecutionContext& context) noexcept
{
    auto ordered = orderedNodes(context);
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
        (*it)->abort(context);
    }

    m_state = MediaGraphSchedulerState::Aborted;
}

void MediaGraphScheduler::clear()
{
    m_nodes.clear();
    m_state = MediaGraphSchedulerState::Idle;
}

MediaGraphSchedulerState MediaGraphScheduler::state() const noexcept
{
    return m_state;
}

bool MediaGraphScheduler::running() const noexcept
{
    return m_state == MediaGraphSchedulerState::Running;
}

std::vector<MediaRuntimeNode*> MediaGraphScheduler::orderedNodes(const MediaGraphExecutionContext& context)
{
    std::vector<MediaRuntimeNode*> result;
    result.reserve(context.executionOrder().size());

    for (MediaNodeId nodeId : context.executionOrder()) {
        if (MediaRuntimeNode* node = findNode(nodeId)) {
            result.push_back(node);
        }
    }

    return result;
}

} // namespace media::ffmpeg::graph

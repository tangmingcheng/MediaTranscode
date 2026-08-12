#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"

#include <algorithm>

namespace media::ffmpeg::graph {

static_assert(noexcept(MediaGraphSchedulerNodeIdHash{}(std::uint32_t{})));
static_assert(noexcept(MediaGraphSchedulerNodeIdEqual{}(
    std::uint32_t{}, std::uint32_t{})));

::media::Status MediaGraphScheduler::registerNode(std::unique_ptr<MediaRuntimeNode> node)
{
    std::vector<std::unique_ptr<MediaRuntimeNode>> nodes;
    nodes.push_back(std::move(node));
    return registerNodes(std::move(nodes));
}

::media::Status MediaGraphScheduler::registerNodes(
    std::vector<std::unique_ptr<MediaRuntimeNode>> nodes)
{
    std::unordered_set<std::uint32_t,
                       MediaGraphSchedulerNodeIdHash,
                       MediaGraphSchedulerNodeIdEqual> preparedIds;
    preparedIds.reserve(nodes.size());
    for (const auto& node : nodes) {
        if (!node || !node->nodeId()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MediaGraphScheduler registerNodes failed: node is invalid"));
        }
        const uint32_t key = node->nodeId().value;
        if (m_nodes.contains(key) || !preparedIds.insert(key).second) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "MediaGraphScheduler registerNodes failed: duplicate runtime node"));
        }
    }

    decltype(m_nodes) preparedNodes;
    preparedNodes.reserve(nodes.size());
    for (auto& node : nodes) {
        const uint32_t key = node->nodeId().value;
        preparedNodes.emplace(key, std::move(node));
    }

    // Allocate the live bucket array before publication. Inserting extracted
    // node handles then reuses their allocated nodes and cannot partially
    // publish because uint32_t hashing/equality are non-throwing.
    m_nodes.reserve(m_nodes.size() + preparedNodes.size());
    while (!preparedNodes.empty()) {
        auto node = preparedNodes.extract(preparedNodes.begin());
        m_nodes.insert(std::move(node));
    }
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

std::vector<MediaRuntimeNode*> MediaGraphScheduler::orderedRuntimeNodes(const MediaGraphExecutionContext& context)
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

std::vector<const MediaRuntimeNode*> MediaGraphScheduler::orderedRuntimeNodes(const MediaGraphExecutionContext& context) const
{
    std::vector<const MediaRuntimeNode*> result;
    result.reserve(context.executionOrder().size());

    for (MediaNodeId nodeId : context.executionOrder()) {
        if (const MediaRuntimeNode* node = findNode(nodeId)) {
            result.push_back(node);
        }
    }

    return result;
}

::media::Status MediaGraphScheduler::configure(MediaGraphExecutionContext& context)
{
    if (!context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphScheduler configure failed: context is not compiled"));
    }

    for (MediaRuntimeNode* node : orderedRuntimeNodes(context)) {
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

    m_finishedNodes.clear();
    for (MediaRuntimeNode* node : orderedRuntimeNodes(context)) {
        auto status = node->start(context);
        if (!status) {
            return status;
        }
    }

    m_state = MediaGraphSchedulerState::Running;
    return ::media::Status::success();
}

::media::Status MediaGraphScheduler::processSchedulingStep(MediaGraphExecutionContext& context)
{
    if (m_state != MediaGraphSchedulerState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphScheduler processSchedulingStep failed: scheduler is not running"));
    }

    for (MediaRuntimeNode* node : orderedRuntimeNodes(context)) {
        if (m_finishedNodes.contains(node->nodeId().value)) {
            continue;
        }
        auto result = node->process(context);
        if (!result) {
            return ::media::Status::failure(result.error());
        }
        if (result.value().state == MediaNodeProcessState::Finished) {
            m_finishedNodes.insert(node->nodeId().value);
        }
    }

    return ::media::Status::success();
}

::media::Status MediaGraphScheduler::flush(MediaGraphExecutionContext& context)
{
    auto ordered = orderedRuntimeNodes(context);
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

    auto ordered = orderedRuntimeNodes(context);
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
    auto ordered = orderedRuntimeNodes(context);
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
        (*it)->abort(context);
    }

    m_state = MediaGraphSchedulerState::Aborted;
}

void MediaGraphScheduler::clear(const MediaGraphExecutionContext* context)
{
    if (context) {
        clear(context->executionOrder());
        return;
    }
    m_nodes.clear();
    m_finishedNodes.clear();
    m_state = MediaGraphSchedulerState::Idle;
}

void MediaGraphScheduler::clear(const std::vector<MediaNodeId>& executionOrder)
{
    for (auto it = executionOrder.rbegin(); it != executionOrder.rend(); ++it) {
        auto nodeIt = m_nodes.find(it->value);
        if (nodeIt == m_nodes.end()) {
            continue;
        }
        std::unique_ptr<MediaRuntimeNode> node = std::move(nodeIt->second);
        m_nodes.erase(nodeIt);
        node.reset();
    }
    m_nodes.clear();
    m_finishedNodes.clear();
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

} // namespace media::ffmpeg::graph

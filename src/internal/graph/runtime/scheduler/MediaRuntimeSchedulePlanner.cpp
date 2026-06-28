#include "internal/graph/runtime/scheduler/MediaRuntimeSchedulePlanner.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

::media::Result<MediaRuntimeSchedulePlan> MediaRuntimeSchedulePlanner::plan(
    const MediaGraphExecutionContext& context,
    const MediaRuntimeSchedulerPolicy& policy)
{
    if (!context.compiled()) {
        return ::media::Result<MediaRuntimeSchedulePlan>::failure(
            ::media::ErrorInfo::notInitialized("MediaRuntimeSchedulePlanner failed: context is not compiled"));
    }

    MediaRuntimeSchedulePlan result;
    result.strategy = policy.strategy;
    result.threadingPolicy = policy.threadingPolicy;

    std::vector<MediaNodeId> order = context.executionOrder();
    if (policy.strategy == MediaRuntimeScheduleStrategy::ReverseGraphOrder) {
        std::reverse(order.begin(), order.end());
    }

    const std::size_t workerCount = policy.threadingPolicy.maxWorkerThreads > 0
        ? policy.threadingPolicy.maxWorkerThreads
        : 1;

    result.steps.reserve(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        MediaRuntimeScheduleStep step;
        step.nodeId = order[i];
        step.parallelizable = policy.allowParallelNodes || policy.threadingPolicy.threaded();
        step.workerIndex = step.parallelizable ? (i % workerCount) : 0;
        result.steps.push_back(step);
    }

    return ::media::Result<MediaRuntimeSchedulePlan>::success(std::move(result));
}

} // namespace media::ffmpeg::graph

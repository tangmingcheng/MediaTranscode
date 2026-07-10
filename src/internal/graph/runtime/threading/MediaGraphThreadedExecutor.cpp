#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"

namespace media::ffmpeg::graph {
void MediaGraphThreadedExecutor::setPolicy(MediaThreadingPolicy policy) noexcept
{
    m_policy = policy;
}

const MediaThreadingPolicy& MediaGraphThreadedExecutor::policy() const noexcept
{
    return m_policy;
}

::media::Status MediaGraphThreadedExecutor::start(MediaGraphExecutionContext& context,
                                                   MediaGraphScheduler& scheduler)
{
    if (!context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphThreadedExecutor start failed: context is not compiled"));
    }

    if (m_state == MediaGraphThreadedExecutorState::Running) {
        return ::media::Status::success();
    }

    m_state = MediaGraphThreadedExecutorState::Starting;

    auto startStatus = scheduler.start(context);
    if (!startStatus) {
        m_state = MediaGraphThreadedExecutorState::Idle;
        return startStatus;
    }

    MediaGraphWorkerConfig workerConfig;
    const auto runtimeNodes = scheduler.orderedRuntimeNodes(context);
    m_workers.clear();
    m_workers.reserve(runtimeNodes.size());

    // Construct every worker before any worker thread is allowed to run. Worker
    // construction binds its node wakeup in the execution context; interleaving
    // that registry mutation with a running worker is undefined behaviour.
    for (MediaRuntimeNode* node : runtimeNodes) {
        if (!node) {
            continue;
        }

        m_workers.push_back(std::make_unique<MediaGraphWorker>(*node, context, workerConfig));
    }

    for (auto& worker : m_workers) {
        auto status = worker->start();
        if (!status) {
            abort(context, scheduler);
            return status;
        }
    }

    m_state = MediaGraphThreadedExecutorState::Running;
    refreshMetrics();
    return ::media::Status::success();
}

::media::Status MediaGraphThreadedExecutor::stop(MediaGraphExecutionContext& context,
                                                  MediaGraphScheduler& scheduler)
{
    if (m_state == MediaGraphThreadedExecutorState::Stopped ||
        m_state == MediaGraphThreadedExecutorState::Idle) {
        m_state = MediaGraphThreadedExecutorState::Stopped;
        return ::media::Status::success();
    }

    m_state = MediaGraphThreadedExecutorState::Stopping;

    for (auto& worker : m_workers) {
        if (worker) {
            worker->requestStop();
        }
    }

    auto closeStatus = MediaGraphLifecycle::closeChannels(context);
    if (!closeStatus) {
        return closeStatus;
    }

    for (auto& worker : m_workers) {
        if (worker) {
            worker->join();
        }
    }

    auto status = scheduler.stop(context);
    if (!status) {
        return status;
    }

    m_state = MediaGraphThreadedExecutorState::Stopped;
    refreshMetrics();
    return ::media::Status::success();
}

void MediaGraphThreadedExecutor::abort(MediaGraphExecutionContext& context,
                                        MediaGraphScheduler& scheduler) noexcept
{
    for (auto& worker : m_workers) {
        if (worker) {
            worker->abort();
        }
    }

    MediaGraphLifecycle::abortChannels(context);

    for (auto& worker : m_workers) {
        if (worker) {
            worker->join();
        }
    }

    scheduler.abort(context);
    m_state = MediaGraphThreadedExecutorState::Aborted;
    refreshMetrics();
}

void MediaGraphThreadedExecutor::clear()
{
    m_workers.clear();
    m_metrics = {};
    m_state = MediaGraphThreadedExecutorState::Idle;
}

MediaGraphThreadedExecutorState MediaGraphThreadedExecutor::state() const noexcept
{
    return m_state;
}

bool MediaGraphThreadedExecutor::running() const noexcept
{
    return m_state == MediaGraphThreadedExecutorState::Running;
}

const MediaGraphRuntimeMetrics& MediaGraphThreadedExecutor::metrics() const noexcept
{
    refreshMetrics();
    return m_metrics;
}

void MediaGraphThreadedExecutor::refreshMetrics() const noexcept
{
    m_metrics.activeWorkers = 0;
    m_metrics.workerIterations = 0;
    m_metrics.workerProcessCalls = 0;
    m_metrics.workerProgress = 0;
    m_metrics.workerWaits = 0;
    m_metrics.workerWakeups = 0;
    m_metrics.workerErrors = 0;

    for (const auto& worker : m_workers) {
        if (!worker) {
            continue;
        }

        if (worker->running()) {
            ++m_metrics.activeWorkers;
        }

        m_metrics.workerProgress += worker->metrics().progress;
        m_metrics.workerProcessCalls += worker->metrics().processCalls;
        m_metrics.workerIterations = m_metrics.workerProcessCalls;
        m_metrics.workerWaits += worker->metrics().waits;
        m_metrics.workerWakeups += worker->metrics().wakeups;
        m_metrics.workerErrors += worker->metrics().errors;
    }
}

} // namespace media::ffmpeg::graph

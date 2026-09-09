#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeMetricsCollector.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"

#include <algorithm>

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

    const auto runtimeNodes = scheduler.orderedRuntimeNodes(context);
    m_workers.clear();
    m_failureRecorder.clear();
    m_workers.reserve(runtimeNodes.size());

    // Construct every worker before any worker thread is allowed to run. Worker
    // construction binds its node wakeup in the execution context; interleaving
    // that registry mutation with a running worker is undefined behaviour.
    for (MediaRuntimeNode* node : runtimeNodes) {
        if (!node) {
            continue;
        }

        m_workers.push_back(std::make_unique<MediaGraphWorker>(
            *node, context, m_failureRecorder, m_failureSupervisor));
    }

    m_failureSupervisor.arm([this, &context] {
        context.cancelSessionPayloadWaiters();
        requestStop(context);
    });

    for (auto& worker : m_workers) {
        auto status = worker->start();
        if (!status) {
            abort(context, scheduler);
            if (auto failure = primaryFailure()) {
                return ::media::Status::failure(failure->error);
            }
            return status;
        }
    }

    if (auto failure = primaryFailure()) {
        abort(context, scheduler);
        return ::media::Status::failure(failure->error);
    }

    m_state = MediaGraphThreadedExecutorState::Running;
    refreshMetrics();
    return ::media::Status::success();
}

void MediaGraphThreadedExecutor::requestStop(MediaGraphExecutionContext& context) noexcept
{
    for (auto& worker : m_workers) if (worker) worker->requestStop();
    for (auto& worker : m_workers) if (worker) worker->interrupt();
    context.interruptNodeWakeups();
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

    requestStop(context);

    auto closeStatus = MediaGraphLifecycle::closeChannels(context);
    if (!closeStatus) {
        return closeStatus;
    }

    for (auto& worker : m_workers) {
        if (worker) {
            worker->join();
        }
    }
    m_failureSupervisor.disarm();

    if (metrics().workerErrors != 0) {
        scheduler.abort(context);
        m_state = MediaGraphThreadedExecutorState::Aborted;
        if (auto failure = primaryFailure()) {
            return ::media::Status::failure(failure->error);
        }
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "MediaGraphThreadedExecutor stop harvested a worker failure; executor aborted"));
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
    m_failureSupervisor.disarm();

    scheduler.abort(context);
    m_state = MediaGraphThreadedExecutorState::Aborted;
    refreshMetrics();
}

void MediaGraphThreadedExecutor::clear()
{
    m_failureSupervisor.disarm();
    m_workers.clear();
    m_failureRecorder.clear();
    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics = {};
    }
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

bool MediaGraphThreadedExecutor::completed() const noexcept
{
    return m_state == MediaGraphThreadedExecutorState::Running &&
           !m_failureRecorder.hasFailure() &&
           !m_workers.empty() &&
           std::all_of(m_workers.begin(), m_workers.end(),
               [](const std::unique_ptr<MediaGraphWorker>& worker) {
                   return worker && worker->finished();
               });
}

bool MediaGraphThreadedExecutor::failed() const noexcept
{
    return m_failureRecorder.hasFailure();
}

std::optional<MediaGraphWorkerFailure>
MediaGraphThreadedExecutor::primaryFailure() const
{
    return m_failureRecorder.primaryFailure();
}

MediaGraphRuntimeMetrics MediaGraphThreadedExecutor::metrics() const noexcept
{
    refreshMetrics();
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_metrics;
}

void MediaGraphThreadedExecutor::refreshMetrics() const noexcept
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    m_metrics = MediaRuntimeMetricsCollector::workers(m_workers);
}

} // namespace media::ffmpeg::graph

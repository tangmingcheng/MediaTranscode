#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeMetricsCollector.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"

#include <algorithm>
#include <exception>

namespace media::ffmpeg::graph {
::media::Result<MediaGraphNodeCompletion> MediaGraphThreadedExecutor::nodeCompletion(MediaNodeId node) const
{
    std::lock_guard owners(m_workersMutex);
    for (const auto& worker : m_workers) {
        if (worker && worker->nodeId() == node) {
            // Exit publication follows finished and finishExecution on the owner.
            const bool exited = worker->exited();
            return ::media::Result<MediaGraphNodeCompletion>::success({worker->finished(), exited});
        }
    }
    return ::media::Result<MediaGraphNodeCompletion>::failure(::media::ErrorInfo::invalidArgument(
        "Output drain references a node without an execution owner"));
}

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
    std::lock_guard lifecycle(m_lifecycleMutex);
    if (!context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphThreadedExecutor start failed: context is not compiled"));
    }

    if (m_state == MediaGraphThreadedExecutorState::Running) {
        return ::media::Status::success();
    }

    struct RollbackContext {
        MediaGraphThreadedExecutor& executor;
        MediaGraphExecutionContext& context;
        MediaGraphScheduler& scheduler;
    } rollbackContext{*this, context, scheduler};
    MediaWorkerStartupGate::Transaction transaction(&rollbackContext, [](void* opaque) noexcept {
        auto& state = *static_cast<RollbackContext*>(opaque);
        state.executor.abort(state.context, state.scheduler);
    });
    const auto fail = [&](const ::media::ErrorInfo& error) noexcept {
        m_failureRecorder.recordScopeFailure(error, MediaGraphWorkerFailurePhase::Preparation);
        transaction.rollback();
        return m_failureRecorder.failureStatus();
    };
    try {
        m_failureRecorder.clear();
        m_failureRecorder.setPhase(MediaGraphWorkerFailurePhase::Preparation);
        m_startup.reset(context.executionOrder().size());
        m_state = MediaGraphThreadedExecutorState::Starting;
        auto prepared = ::media::Status::success();
        {
            std::lock_guard owners(m_workersMutex);
            prepared = m_startup.prepare(context, scheduler, m_workers, m_failureRecorder, m_failureSupervisor);
        }
        if (!prepared) return fail(prepared.error());
        auto started = scheduler.start(context);
        if (!started) return fail(started.error());
        m_failureSupervisor.arm({this, &context, [](void* owner, void* state) noexcept {
            auto& context = *static_cast<MediaGraphExecutionContext*>(state);
            context.cancelSessionPayloadWaiters();
            static_cast<MediaGraphThreadedExecutor*>(owner)->requestStop(context);
        }});
        auto launched = m_startup.launch(m_workers.view());
        if (!launched) return fail(launched.error());
        m_failureRecorder.setPhase(MediaGraphWorkerFailurePhase::Runtime);
        if (!m_startup.commit()) return fail({::media::ErrorCode::Cancelled, 0, {}});
        m_state = MediaGraphThreadedExecutorState::Running;
        refreshMetrics();
        transaction.commit();
        return ::media::Status::success();
    } catch (...) {
        return fail(MediaWorkerStartupGate::currentException());
    }
}

void MediaGraphThreadedExecutor::requestStop(MediaGraphExecutionContext& context) noexcept
{
    m_startup.cancel();
    std::lock_guard owners(m_workersMutex);
    for (auto& worker : m_workers) if (worker) worker->requestStop();
    for (auto& worker : m_workers) if (worker) worker->interrupt();
    context.interruptNodeWakeups();
}

::media::Status MediaGraphThreadedExecutor::stop(MediaGraphExecutionContext& context,
                                                  MediaGraphScheduler& scheduler)
{
    m_startup.cancel();
    std::lock_guard lifecycle(m_lifecycleMutex);
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
    m_startup.cancel();
    std::lock_guard lifecycle(m_lifecycleMutex);
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
    m_startup.cancel();
    std::lock_guard lifecycle(m_lifecycleMutex);
    // A scope must release its owners before their storage can be cleared.
    for (auto& worker : m_workers) worker->requestStop();
    for (auto& worker : m_workers) worker->interrupt();
    for (auto& worker : m_workers) worker->join();
    m_failureSupervisor.disarm();
    { std::lock_guard owners(m_workersMutex); m_workers = {}; }
    m_failureRecorder.clear();
    {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics = {};
    }
    m_state = MediaGraphThreadedExecutorState::Idle;
}

MediaGraphThreadedExecutorState MediaGraphThreadedExecutor::state() const noexcept
{
    return m_state.load();
}

bool MediaGraphThreadedExecutor::running() const noexcept
{
    return m_state == MediaGraphThreadedExecutorState::Running;
}

bool MediaGraphThreadedExecutor::completed() const noexcept
{
    std::lock_guard owners(m_workersMutex);
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
    std::lock_guard owners(m_workersMutex);
    m_metrics = MediaRuntimeMetricsCollector::workers(m_workers.view());
}

} // namespace media::ffmpeg::graph

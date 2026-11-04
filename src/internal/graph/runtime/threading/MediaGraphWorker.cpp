#include "internal/graph/runtime/threading/MediaGraphWorker.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/diagnostics/MediaCurrentThreadCpuClock.h"
#include "internal/graph/runtime/threading/MediaWorkerThreadRegistration.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

#if defined(__linux__)
#include <pthread.h>
#endif

namespace media::ffmpeg::graph {

MediaGraphWorker::MediaGraphWorker(MediaRuntimeNode& node,
                                   MediaGraphExecutionContext& context,
                                   MediaGraphWorkerFailureRecorder& failureRecorder,
                                   MediaGraphWorkerFailureSupervisor& failureSupervisor,
                                   MediaWorkerExecutionPlan execution, MediaWorkerStartupGate& startup)
    : m_node(node)
    , m_context(context)
    , m_wakeup(context.nodeWakeup(node.nodeId()))
    , m_exitToken(context.nodeExitToken(node.nodeId()))
    , m_failureRecorder(&failureRecorder)
    , m_failureSupervisor(&failureSupervisor)
    , m_execution(execution)
    , m_exceptionFailure{node.nodeId(), MediaNodeKind::Unknown, {},
        ::media::ErrorInfo{::media::ErrorCode::InternalError, 0, {}},
        MediaGraphWorkerFailurePhase::Preparation}
    , m_startup(startup)
{
}

MediaGraphWorker::~MediaGraphWorker()
{
    requestStop();
    interrupt();
    join();
    if (m_exitToken && !m_started)
        m_exitToken->m_cancelledBeforeStart.store(true, std::memory_order_release);
}

::media::Status MediaGraphWorker::start()
{
    if (!m_exitToken || m_execution.node != nodeId()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaGraphWorker requires a compiled exit token"));
    }
    if (m_started) {
        return ::media::Status::success();
    }
    if (m_stopRequested || m_aborted) {
        if (!m_started) m_exitToken->m_cancelledBeforeStart.store(true, std::memory_order_release);
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "MediaGraphWorker start was cancelled by coordinated failure"));
    }

    m_finished = false;
    m_exitToken->m_exited.store(false, std::memory_order_release);
    m_exited.store(false, std::memory_order_release);
    try {
        m_thread = std::thread(&MediaGraphWorker::run, this);
        m_started = true;
    } catch (...) {
        m_exited.store(true, std::memory_order_release);
        m_exitToken->m_cancelledBeforeStart.store(true, std::memory_order_release);
        throw;
    }
    return ::media::Status::success();
}

void MediaGraphWorker::requestStop() noexcept
{
    m_startup.cancel();
    m_stopRequested = true;
    m_wakeup.interrupt();
}

void MediaGraphWorker::interrupt() noexcept
{
    if (m_started.load(std::memory_order_acquire)) m_node.interrupt(m_context);
}

void MediaGraphWorker::abort() noexcept
{
    m_startup.cancel();
    m_aborted = true;
    m_stopRequested = true;
    m_wakeup.interrupt();
    interrupt();
}

void MediaGraphWorker::join()
{
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_exitToken && !m_started && (m_stopRequested || m_aborted))
        m_exitToken->m_cancelledBeforeStart.store(true, std::memory_order_release);
}

bool MediaGraphWorker::running() const noexcept
{
    return m_running;
}

bool MediaGraphWorker::finished() const noexcept
{
    return m_finished.load(std::memory_order_acquire);
}

bool MediaGraphWorker::exited() const noexcept
{
    return m_exited.load(std::memory_order_acquire);
}

bool MediaGraphWorker::stopRequested() const noexcept
{
    return m_stopRequested;
}

bool MediaGraphWorker::aborted() const noexcept
{
    return m_aborted;
}

MediaNodeId MediaGraphWorker::nodeId() const noexcept
{
    return m_node.nodeId();
}

const MediaGraphWorkerMetrics& MediaGraphWorker::metrics() const noexcept
{
    return m_metrics;
}

void MediaGraphWorker::recordThreadCpu(
    const ::media::Result<std::uint64_t>& startedAt) noexcept
{
    auto finishedAt = MediaCurrentThreadCpuClock::nowNanoseconds();
    if (startedAt && finishedAt && finishedAt.value() >= startedAt.value()) {
        m_metrics.threadCpuNanoseconds =
            finishedAt.value() - startedAt.value();
        m_metrics.threadCpuMeasurementAvailable = true;
    }
    try {
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "worker.thread_cpu node=" + std::to_string(m_node.nodeId().value) +
                " available=" +
                (m_metrics.threadCpuMeasurementAvailable.load() ? "1" : "0") +
                " cpu_ns=" +
                std::to_string(m_metrics.threadCpuNanoseconds.load()) +
                " process_calls=" +
                std::to_string(m_metrics.processCalls.load()));
    } catch (...) {
        // Diagnostics cannot escape worker teardown.
    }
}

MediaGraphWorker::FailureDisposition MediaGraphWorker::recordFailure(
    const ::media::ErrorInfo& error, std::optional<MediaGraphWorkerFailurePhase> phase) noexcept
{
    bool primary = false;
    try {
        const MediaGraph* graph = m_context.graph();
        const MediaNode* node = graph ? graph->findNode(m_node.nodeId()) : nullptr;
        primary = m_failureRecorder->recordFirst({m_node.nodeId(),
            node ? node->kind : MediaNodeKind::Unknown,
            node ? (!node->diagnosticName.empty() ? node->diagnosticName : node->name) : std::string{},
            error, phase.value_or(m_failureRecorder->phase())});
    } catch (...) {
        // Constructed before thread creation. Moving this record cannot allocate;
        // preserve the native error even when copying its text exhausted memory.
        m_exceptionFailure.error.code = error.code;
        m_exceptionFailure.error.nativeCode = error.nativeCode;
        m_exceptionFailure.phase = phase.value_or(m_failureRecorder->phase());
        primary = m_failureRecorder->recordFirst(std::move(m_exceptionFailure));
    }
    if (!primary && error.code == ::media::ErrorCode::Cancelled &&
        (stopRequested() || aborted())) return FailureDisposition::CoordinatedCancellation;
    try {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string(primary ? "worker.failed node=" : "worker.secondary_failure node=") +
                std::to_string(m_node.nodeId().value) + " error=" + error.describe());
    } catch (...) {
        // The authoritative first-error record is already published. Diagnostic
        // allocation must not suppress coordinated stop or owner cleanup.
    }
    if (primary && m_failureSupervisor) m_failureSupervisor->notifyPrimaryFailure();
    return primary ? FailureDisposition::Primary : FailureDisposition::Secondary;
}

void MediaGraphWorker::recordWaitOutcome(MediaNodeWakeup::WaitOutcome outcome)
{
    switch (outcome) {
    case MediaNodeWakeup::WaitOutcome::Notified:
        ++m_metrics.wakeups;
        break;
    case MediaNodeWakeup::WaitOutcome::Deadline:
        ++m_metrics.deadlines;
        break;
    case MediaNodeWakeup::WaitOutcome::Interrupted:
        if (!m_stopRequested && !m_aborted) {
            if (recordFailure(::media::ErrorInfo::cancelled(
                    "worker wakeup was interrupted outside its coordinated stop")) !=
                FailureDisposition::CoordinatedCancellation) ++m_metrics.errors;
            m_aborted = true;
        }
        m_stopRequested = true;
        break;
    }
}

void MediaGraphWorker::run()
{
    struct ExitPublication {
        std::atomic_bool& exited;
        std::atomic_bool& tokenExited;
        ~ExitPublication() {
            exited.store(true, std::memory_order_release);
            tokenExited.store(true, std::memory_order_release);
        }
    } exitPublication{m_exited, m_exitToken->m_exited};
    MediaWorkerThreadRegistration registration;
    struct OwnerRelease {
        MediaGraphWorker& worker;
        MediaWorkerThreadRegistration& registration;
        ~OwnerRelease() {
            auto completed = worker.m_node.finishExecution(worker.m_context);
            if (!completed && !(completed.error().code == ::media::ErrorCode::Cancelled &&
                                (worker.stopRequested() || worker.aborted()))) {
                if (worker.recordFailure(completed.error(), MediaGraphWorkerFailurePhase::Release) !=
                    FailureDisposition::CoordinatedCancellation) ++worker.m_metrics.errors;
            }
            try {
                auto released = registration.leave();
                if (!released && worker.recordFailure(released.error(), MediaGraphWorkerFailurePhase::Release) !=
                    FailureDisposition::CoordinatedCancellation) ++worker.m_metrics.errors;
            } catch (...) {
                worker.recordFailure(MediaWorkerStartupGate::currentException(), MediaGraphWorkerFailurePhase::Release);
                ++worker.m_metrics.errors;
            }
        }
    } ownerRelease{*this, registration};
    try {
        auto prepared = registration.enter(m_execution);
        if (!prepared) {
            recordFailure(prepared.error(), MediaGraphWorkerFailurePhase::Preparation);
            ++m_metrics.errors;
            m_startup.cancel();
            return;
        }
        m_startup.ready();
        if (!m_startup.awaitCommit() || m_stopRequested || m_aborted) return;
    } catch (...) {
        m_startup.cancel();
        recordFailure(MediaWorkerStartupGate::currentException(), MediaGraphWorkerFailurePhase::Preparation);
        ++m_metrics.errors;
        return;
    }
    try {
    const auto threadCpuStartedAt =
        MediaCurrentThreadCpuClock::nowNanoseconds();
#if defined(__linux__)
    char threadName[16]{};
    std::snprintf(
        threadName,
        sizeof(threadName),
        "mt-n%u",
        static_cast<unsigned int>(m_node.nodeId().value));
    pthread_setname_np(pthread_self(), threadName);
#endif
    m_running = true;
    m_wakeup.reset();

    while (!m_stopRequested && !m_aborted) {
        const MediaNodeWakeup::Sequence observedSequence = m_wakeup.sequence();
        ++m_metrics.processCalls;
        auto result = m_node.process(m_context);

        const bool cancellationRequested = m_stopRequested || m_aborted;
        const bool interruptedByCancellation =
            !result && result.error().code == ::media::ErrorCode::Cancelled;
        if (cancellationRequested && interruptedByCancellation) {
            break;
        }

        if (!result) {
            const auto disposition = recordFailure(result.error());
            if (disposition != FailureDisposition::CoordinatedCancellation) {
                ++m_metrics.errors;
            }
            m_aborted = true;
            break;
        }

        switch (result.value().state) {
        case MediaNodeProcessState::Progress:
            ++m_metrics.progress;
            break;
        case MediaNodeProcessState::Waiting:
            ++m_metrics.waits;
            if (result.value().deadlineWait) {
                const auto& deadlineWait = *result.value().deadlineWait;
                if (const auto* steady = std::get_if<
                        MediaNodeProcessResult::DeadlineWait::Steady>(
                            &deadlineWait.deadline)) {
                    const auto now = std::chrono::steady_clock::now();
                    const auto timeout = steady->deadline > now
                        ? std::chrono::duration_cast<std::chrono::nanoseconds>(
                              steady->deadline - now)
                        : std::chrono::nanoseconds{0};
                    const auto waited = m_wakeup.wait(
                        observedSequence, deadlineWait.wakePolicy, timeout, nullptr);
                    if (!waited) {
                        if (recordFailure(waited.error()) !=
                            FailureDisposition::CoordinatedCancellation) {
                            ++m_metrics.errors;
                        }
                        m_aborted = true;
                        break;
                    }
                    recordWaitOutcome(waited.value());
                    break;
                }
                const auto& avSync = std::get<
                    MediaNodeProcessResult::DeadlineWait::AvSyncMaster>(
                        deadlineWait.deadline);
                auto group = m_context.findAvSyncGroup(avSync.syncGroup);
                if (!group) {
                    if (recordFailure(::media::ErrorInfo::notInitialized(
                            "MediaGraphWorker deadline wait requires a registered A/V sync group")) !=
                        FailureDisposition::CoordinatedCancellation) {
                        ++m_metrics.errors;
                    }
                    m_aborted = true;
                    break;
                }
                auto now = group->clock()->now();
                if (!now) {
                    if (recordFailure(now.error()) !=
                        FailureDisposition::CoordinatedCancellation) ++m_metrics.errors;
                    m_aborted = true;
                    break;
                }
                auto remaining = avSync.deadline.checkedSubtract(now.value());
                if (!remaining) {
                    if (recordFailure(remaining.error()) !=
                        FailureDisposition::CoordinatedCancellation) ++m_metrics.errors;
                    m_aborted = true;
                    break;
                }
                const auto timeout = std::chrono::nanoseconds(
                    std::max<std::int64_t>(0, remaining.value().nanoseconds()));
                const auto waited = m_wakeup.wait(
                    observedSequence, deadlineWait.wakePolicy, timeout, nullptr);
                if (!waited) {
                    if (recordFailure(waited.error()) !=
                        FailureDisposition::CoordinatedCancellation) {
                        ++m_metrics.errors;
                    }
                    m_aborted = true;
                    break;
                }
                recordWaitOutcome(waited.value());
            } else {
                const auto waited = m_wakeup.wait(
                    observedSequence,
                    MediaNodeDeadlineWakePolicy::InputOrDeadline, std::nullopt, nullptr);
                if (!waited) {
                    if (recordFailure(waited.error()) !=
                        FailureDisposition::CoordinatedCancellation) {
                        ++m_metrics.errors;
                    }
                    m_aborted = true;
                } else {
                    recordWaitOutcome(waited.value());
                }
            }
            break;
        case MediaNodeProcessState::Finished:
            mediaGraphDiagnosticLog(
                MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeLifecycle,
                "worker.finished node=" +
                    std::to_string(m_node.nodeId().value));
            m_finished.store(true, std::memory_order_release);
            m_running = false;
            recordThreadCpu(threadCpuStartedAt);
            return;
        }
    }

    m_running = false;
    recordThreadCpu(threadCpuStartedAt);
    } catch (...) {
        m_running = false;
        m_aborted = true;
        recordFailure(MediaWorkerStartupGate::currentException());
        ++m_metrics.errors;
    }
}

} // namespace media::ffmpeg::graph

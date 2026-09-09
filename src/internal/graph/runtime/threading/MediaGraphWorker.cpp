#include "internal/graph/runtime/threading/MediaGraphWorker.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/diagnostics/MediaCurrentThreadCpuClock.h"

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
                                   MediaGraphExecutionContext& context)
    : m_node(node)
    , m_context(context)
    , m_wakeup(context.nodeWakeup(node.nodeId()))
    , m_exitToken(context.nodeExitToken(node.nodeId()))
    , m_failureRecorder(&m_localFailureRecorder)
{
}

MediaGraphWorker::MediaGraphWorker(MediaRuntimeNode& node,
                                   MediaGraphExecutionContext& context,
                                   MediaGraphWorkerFailureRecorder& failureRecorder,
                                   MediaGraphWorkerFailureSupervisor& failureSupervisor)
    : m_node(node)
    , m_context(context)
    , m_wakeup(context.nodeWakeup(node.nodeId()))
    , m_exitToken(context.nodeExitToken(node.nodeId()))
    , m_failureRecorder(&failureRecorder)
    , m_failureSupervisor(&failureSupervisor)
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
    if (!m_exitToken) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaGraphWorker requires a compiled exit token"));
    }
    if (m_running) {
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
    m_stopRequested = true;
    m_wakeup.interrupt();
}

void MediaGraphWorker::interrupt() noexcept
{
    m_node.interrupt(m_context);
}

void MediaGraphWorker::abort() noexcept
{
    m_aborted = true;
    m_stopRequested = true;
    m_wakeup.interrupt();
    m_node.interrupt(m_context);
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
    ::media::ErrorInfo error)
{
    const MediaGraph* graph = m_context.graph();
    const MediaNode* node = graph ? graph->findNode(m_node.nodeId()) : nullptr;
    const MediaNodeKind nodeKind = node ? node->kind : MediaNodeKind::Unknown;
    const std::string nodeName = node
        ? (!node->diagnosticName.empty() ? node->diagnosticName : node->name)
        : std::string{};
    const ::media::ErrorInfo diagnosticError = error;
    const bool primary = m_failureRecorder->recordFirst(
        MediaGraphWorkerFailure{ m_node.nodeId(), nodeKind, nodeName, std::move(error) });
    if (!primary) {
        if (diagnosticError.code == ::media::ErrorCode::Cancelled &&
            (stopRequested() || aborted())) {
            return FailureDisposition::CoordinatedCancellation;
        }
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "worker.secondary_failure node=" +
                std::to_string(m_node.nodeId().value) +
                " kind=" + mediaGraphDiagnosticNodeKindName(nodeKind) +
                " name=" + nodeName +
                " error=" + diagnosticError.describe());
        return FailureDisposition::Secondary;
    }
    mediaGraphDiagnosticLog(
        MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "worker.failed node=" + std::to_string(m_node.nodeId().value) +
            " kind=" + mediaGraphDiagnosticNodeKindName(nodeKind) +
            " name=" + nodeName +
            " error=" + diagnosticError.describe());
    if (m_failureSupervisor) {
        m_failureSupervisor->notifyPrimaryFailure();
    }
    return FailureDisposition::Primary;
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
        MediaGraphWorker& worker;
        std::atomic_bool& exited;
        std::atomic_bool& tokenExited;
        ~ExitPublication() {
            auto completed = worker.m_node.finishExecution(worker.m_context);
            if (!completed && !(completed.error().code == ::media::ErrorCode::Cancelled &&
                                (worker.stopRequested() || worker.aborted()))) {
                if (worker.recordFailure(completed.error()) !=
                    FailureDisposition::CoordinatedCancellation) ++worker.m_metrics.errors;
            }
            exited.store(true, std::memory_order_release);
            tokenExited.store(true, std::memory_order_release);
        }
    } exitPublication{*this, m_exited, m_exitToken->m_exited};
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
                        observedSequence, deadlineWait.wakePolicy, timeout);
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
                    observedSequence, deadlineWait.wakePolicy, timeout);
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
                    MediaNodeDeadlineWakePolicy::InputOrDeadline);
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
}

} // namespace media::ffmpeg::graph

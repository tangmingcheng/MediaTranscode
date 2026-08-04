#include "internal/graph/runtime/threading/MediaGraphWorker.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

MediaGraphWorker::MediaGraphWorker(MediaRuntimeNode& node,
                                   MediaGraphExecutionContext& context,
                                   MediaGraphWorkerConfig config)
    : m_node(node)
    , m_context(context)
    , m_wakeup(context.nodeWakeup(node.nodeId()))
    , m_failureRecorder(&m_localFailureRecorder)
    , m_config(config)
{
}

MediaGraphWorker::MediaGraphWorker(MediaRuntimeNode& node,
                                   MediaGraphExecutionContext& context,
                                   MediaGraphWorkerFailureRecorder& failureRecorder,
                                   MediaGraphWorkerFailureSupervisor& failureSupervisor,
                                   MediaGraphWorkerConfig config)
    : m_node(node)
    , m_context(context)
    , m_wakeup(context.nodeWakeup(node.nodeId()))
    , m_failureRecorder(&failureRecorder)
    , m_failureSupervisor(&failureSupervisor)
    , m_config(config)
{
}

MediaGraphWorker::~MediaGraphWorker()
{
    requestStop();
    interrupt();
    join();
}

::media::Status MediaGraphWorker::start()
{
    if (m_running) {
        return ::media::Status::success();
    }
    if (m_stopRequested || m_aborted) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "MediaGraphWorker start was cancelled by coordinated failure"));
    }

    m_finished = false;
    m_thread = std::thread(&MediaGraphWorker::run, this);
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
}

bool MediaGraphWorker::running() const noexcept
{
    return m_running;
}

bool MediaGraphWorker::finished() const noexcept
{
    return m_finished.load(std::memory_order_acquire);
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

void MediaGraphWorker::run()
{
    m_running = true;
    uint32_t consecutiveErrors = 0;
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
                ++consecutiveErrors;
            }
            if (consecutiveErrors >= m_config.maxConsecutiveErrors) {
                m_aborted = true;
                break;
            }
        } else {
            consecutiveErrors = 0;
        }

        if (!result) {
            continue;
        }

        switch (result.value().state) {
        case MediaNodeProcessState::Progress:
            ++m_metrics.progress;
            break;
        case MediaNodeProcessState::Waiting:
            ++m_metrics.waits;
            if (result.value().deadlineWait) {
                const auto& deadlineWait = *result.value().deadlineWait;
                auto group = m_context.findAvSyncGroup(deadlineWait.syncGroup);
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
                auto remaining = deadlineWait.masterDeadline.checkedSubtract(now.value());
                if (!remaining) {
                    if (recordFailure(remaining.error()) !=
                        FailureDisposition::CoordinatedCancellation) ++m_metrics.errors;
                    m_aborted = true;
                    break;
                }
                const auto timeout = std::chrono::nanoseconds(
                    std::max<std::int64_t>(0, remaining.value().nanoseconds()));
                const auto outcome = m_wakeup.wait(observedSequence, timeout);
                if (outcome == MediaNodeWakeup::WaitOutcome::Notified) {
                    ++m_metrics.wakeups;
                } else if (outcome == MediaNodeWakeup::WaitOutcome::Deadline) {
                    ++m_metrics.deadlines;
                }
            } else if (m_wakeup.wait(observedSequence) ==
                       MediaNodeWakeup::WaitOutcome::Notified) {
                ++m_metrics.wakeups;
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
            return;
        }
    }

    m_running = false;
}

} // namespace media::ffmpeg::graph

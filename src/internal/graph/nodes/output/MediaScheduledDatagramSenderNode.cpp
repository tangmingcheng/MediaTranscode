#include "internal/graph/nodes/output/MediaScheduledDatagramSenderNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <sstream>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {

MediaScheduledDatagramSenderNode::MediaScheduledDatagramSenderNode(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSession,
    MediaTranscodeStreamSet streamSet,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaScheduledDatagramSenderNode"),
      m_plannedSession(std::move(plannedSession)),
      m_streamSet(streamSet),
      m_authority(std::move(authority))
{
}

MediaScheduledDatagramSenderNode::~MediaScheduledDatagramSenderNode() = default;

::media::Result<std::unique_ptr<MediaScheduledDatagramSenderNode>>
MediaScheduledDatagramSenderNode::create(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSession,
    MediaTranscodeStreamSet streamSet,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
{
    using Result = ::media::Result<std::unique_ptr<MediaScheduledDatagramSenderNode>>;
    if (!nodeId.isValid() || !plannedSession.valid() || !authority ||
        authority->sessionKey() != plannedSession ||
        authority->streamSet() != streamSet) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender requires exact output authority"));
    }
    auto node = std::unique_ptr<MediaScheduledDatagramSenderNode>(
        new (std::nothrow) MediaScheduledDatagramSenderNode(
            nodeId, std::move(plannedSession), streamSet,
            std::move(authority)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaScheduledDatagramSenderNode"));
    }
    return Result::success(std::move(node));
}

MediaNodeKind MediaScheduledDatagramSenderNode::staticKind() noexcept
{
    return MediaNodeKind::ScheduledDatagramSender;
}

::media::Status MediaScheduledDatagramSenderNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const MediaChannel* plan = context.findInputChannel(nodeId(), "plan");
    const MediaChannel* batch = context.findInputChannel(nodeId(), "batch");
    if (context.inputChannels(nodeId()).size() != 2 ||
        !context.outputChannels(nodeId()).empty() || !plan || !batch ||
        plan->binding().streamKind != MediaStreamKind::Metadata ||
        batch->binding().streamKind != MediaStreamKind::Metadata) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender requires exact plan and batch inputs"));
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::start(
    MediaGraphExecutionContext& context)
{
    closeSender();
    m_generation.reset();
    m_pacing.reset();
    m_previousPlannedCompletion.reset();
    m_scheduledBatchMaximumBytes = 0;
    m_terminalFailure.reset();
    m_wakeup.reset();
    m_forwardPacer.reset();
    m_maximumEnqueueLateness = MediaRunningTime::fromNanoseconds(0);
    m_maximumWakeOvershoot = MediaRunningTime::fromNanoseconds(0);
    m_maximumSendDuration = MediaRunningTime::fromNanoseconds(0);
    m_maximumForwardShift = MediaRunningTime::fromNanoseconds(0);
    m_cumulativeWaitDuration = MediaRunningTime::fromNanoseconds(0);
    m_cumulativeSendDuration = MediaRunningTime::fromNanoseconds(0);
    m_batches = 0;
    m_datagrams = 0;
    m_bytes = 0;
    m_enqueueDeadlineMisses = 0;
    m_diagnosticsEmitted = false;
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Status MediaScheduledDatagramSenderNode::bindPlan(
    const MediaProjectMpegTsRuntimePlanBuffer& plan)
{
    auto currentActivation = m_authority
        ? m_authority->currentActivation()
        : ::media::Result<MediaProtocolOutputActivation>::failure(
              ::media::ErrorInfo::notInitialized(
                  "scheduled datagram sender has no output authority"));
    if (!currentActivation || plan.sessionKey() != m_plannedSession ||
        plan.streamSet() != m_streamSet || !m_authority ||
        currentActivation.value() != plan.activation()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender plan differs from output authority"));
    }
    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
        &plan.outputPlan().transport);
    auto sharedNtp = m_authority->sharedNtpEpoch();
    if (!rtp || !sharedNtp ||
        plan.outputPlan().scheduledBatchMaximumBytes == 0 ||
        rtp->pacing().execution !=
            MediaDatagramDispatchExecution::UserspaceWaitAndSend ||
        rtp->pacing().evidence !=
            MediaDatagramTimingEvidence::UserspaceSendReturn ||
        rtp->pacing().deadlinePolicy !=
            MediaDatagramDeadlinePolicy::CanonicalOrdered ||
        (m_generation && plan.activation().generation <= *m_generation)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender requires a new complete MP2T RTP plan"));
    }
    auto continuity = MediaMpegTsRtpContinuityState::create(
        rtp->initialSequenceNumber());
    if (!continuity) return ::media::Status::failure(continuity.error());
    auto sockets = MediaSocketRuntime::create();
    if (!sockets) return ::media::Status::failure(sockets.error());
    MediaUdpDatagramSenderSocketFactory portFactory(
        std::move(sockets).value());
    auto sink = MediaMpegTsRtpDatagramSink::create(
        *rtp, plan.activation(), *sharedNtp,
        std::move(continuity).value(), portFactory);
    if (!sink) return ::media::Status::failure(sink.error());
    closeSender();
    m_sink = std::move(sink).value();
    m_generation = plan.activation().generation;
    m_pacing = rtp->pacing();
    m_previousPlannedCompletion.reset();
    m_forwardPacer.reset();
    m_scheduledBatchMaximumBytes =
        plan.outputPlan().scheduledBatchMaximumBytes;
    return ::media::Status::success();
}

::media::Status MediaScheduledDatagramSenderNode::waitUntil(
    MediaRunningTime deadline)
{
    while (true) {
        auto now = m_authority->now();
        if (!now) return ::media::Status::failure(now.error());
        if (now.value() >= deadline) return ::media::Status::success();
        const auto remaining = deadline.nanoseconds() - now.value().nanoseconds();
        const auto sequence = m_wakeup.sequence();
        auto waited = m_wakeup.wait(
            sequence, MediaNodeDeadlineWakePolicy::DeadlineOrCancellation,
            std::chrono::nanoseconds(remaining));
        if (!waited) return ::media::Status::failure(waited.error());
        if (waited.value() == MediaNodeWakeup::WaitOutcome::Interrupted) {
            return ::media::Status::failure(::media::ErrorInfo::cancelled(
                "scheduled datagram sender wait was interrupted"));
        }
    }
}

::media::Status MediaScheduledDatagramSenderNode::sendBatch(
    const MediaScheduledDatagramBatchBuffer& batch)
{
    const auto payloadBytes = batch.payloadFootprintBytes();
    if (!m_sink || !m_generation || !m_pacing ||
        batch.generation() != *m_generation ||
        !payloadBytes || *payloadBytes == 0 ||
        *payloadBytes > m_scheduledBatchMaximumBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram batch violates the active generation or byte contract"));
    }
    for (const auto& datagram : batch.datagrams()) {
        if (m_previousPlannedCompletion &&
            datagram.enqueueNotBefore() < *m_previousPlannedCompletion) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "scheduled datagram global enqueue reservations overlap"));
        }
        auto plannedCompletion = datagram.enqueueNotBefore().checkedAdd(
            datagram.serviceDuration());
        if (!plannedCompletion) {
            return ::media::Status::failure(plannedCompletion.error());
        }
        auto effectiveEligibility = m_forwardPacer.prepare(
            datagram.enqueueNotBefore(), datagram.enqueueDeadline(),
            datagram.serviceDuration());
        if (!effectiveEligibility) {
            return ::media::Status::failure(effectiveEligibility.error());
        }
        auto forwardShift = effectiveEligibility.value().checkedSubtract(
            datagram.enqueueNotBefore());
        if (!forwardShift) {
            return ::media::Status::failure(forwardShift.error());
        }
        m_maximumForwardShift = (std::max)(
            m_maximumForwardShift, forwardShift.value());
        auto waitStarted = m_authority->now();
        if (!waitStarted) return ::media::Status::failure(waitStarted.error());
        auto waited = waitUntil(effectiveEligibility.value());
        if (!waited) return waited;
        auto before = m_authority->now();
        if (!before) return ::media::Status::failure(before.error());
        auto waitDuration = before.value().checkedSubtract(waitStarted.value());
        auto wakeOvershoot = before.value() > effectiveEligibility.value()
            ? before.value().checkedSubtract(effectiveEligibility.value())
            : ::media::Result<MediaRunningTime>::success(
                  MediaRunningTime::fromNanoseconds(0));
        if (!waitDuration || !wakeOvershoot) {
            return ::media::Status::failure(
                !waitDuration ? waitDuration.error() :
                wakeOvershoot.error());
        }
        auto cumulativeWait = m_cumulativeWaitDuration.checkedAdd(
            waitDuration.value());
        if (!cumulativeWait) return ::media::Status::failure(cumulativeWait.error());
        m_cumulativeWaitDuration = cumulativeWait.value();
        m_maximumWakeOvershoot = (std::max)(
            m_maximumWakeOvershoot, wakeOvershoot.value());
        const bool missedBefore =
            before.value() > datagram.enqueueDeadline();
        auto sent = m_sink->enqueue(datagram.bytes(), before.value());
        if (!sent) return ::media::Status::failure(sent.error());
        auto actualEnqueue = m_authority->now();
        if (!actualEnqueue) return ::media::Status::failure(actualEnqueue.error());
        auto sendDuration = actualEnqueue.value().checkedSubtract(before.value());
        auto cumulativeSend = sendDuration
            ? m_cumulativeSendDuration.checkedAdd(sendDuration.value())
            : ::media::Result<MediaRunningTime>::failure(sendDuration.error());
        if (!sendDuration || !cumulativeSend) {
            return ::media::Status::failure(
                sendDuration ? cumulativeSend.error() : sendDuration.error());
        }
        m_cumulativeSendDuration = cumulativeSend.value();
        m_maximumSendDuration = (std::max)(
            m_maximumSendDuration, sendDuration.value());
        const bool missedAfter =
            actualEnqueue.value() > datagram.enqueueDeadline();
        auto committed = m_forwardPacer.commitSuccessfulSubmit(before.value());
        if (!committed) return committed;
        if ((missedBefore || missedAfter) &&
            m_enqueueDeadlineMisses ==
                (std::numeric_limits<std::uint64_t>::max)()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "scheduled datagram deadline miss counter overflowed"));
        }
        if (missedBefore || missedAfter) ++m_enqueueDeadlineMisses;
        const auto lateness = actualEnqueue.value() > datagram.enqueueNotBefore()
            ? actualEnqueue.value().checkedSubtract(datagram.enqueueNotBefore())
            : ::media::Result<MediaRunningTime>::success(
                  MediaRunningTime::fromNanoseconds(0));
        if (!lateness) return ::media::Status::failure(lateness.error());
        if (lateness.value() > m_maximumEnqueueLateness) {
            m_maximumEnqueueLateness = lateness.value();
        }
        m_previousPlannedCompletion = plannedCompletion.value();
        if (m_datagrams == std::numeric_limits<std::uint64_t>::max() ||
            sent.value() > std::numeric_limits<std::uint64_t>::max() - m_bytes) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "scheduled datagram sender counters overflowed"));
        }
        ++m_datagrams;
        m_bytes += static_cast<std::uint64_t>(sent.value());
    }
    if (m_batches == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram sender batch counter overflowed"));
    }
    ++m_batches;
    return ::media::Status::success();
}

void MediaScheduledDatagramSenderNode::emitDiagnostics(
    const char* stage) noexcept
{
    if (m_diagnosticsEmitted) return;
    m_diagnosticsEmitted = true;
    try {
        std::ostringstream diagnostic;
        diagnostic << "scheduled_datagram_sender stage=" << stage
                   << " generation=" << m_generation.value_or(0)
                   << " batches=" << m_batches
                   << " datagrams=" << m_datagrams
                   << " bytes=" << m_bytes
                   << " actual_enqueue_max_lateness_ns="
                   << m_maximumEnqueueLateness.nanoseconds()
                   << " maximum_wake_overshoot_ns="
                   << m_maximumWakeOvershoot.nanoseconds()
                   << " cumulative_wait_ns="
                   << m_cumulativeWaitDuration.nanoseconds()
                   << " maximum_send_duration_ns="
                   << m_maximumSendDuration.nanoseconds()
                   << " maximum_forward_shift_ns="
                   << m_maximumForwardShift.nanoseconds()
                   << " cumulative_send_duration_ns="
                   << m_cumulativeSendDuration.nanoseconds()
                   << " enqueue_deadline_misses="
                   << m_enqueueDeadlineMisses
                   << " wire_completion_evidence=unavailable";
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            diagnostic.str());
    } catch (...) {
        // Diagnostics are best-effort and cannot escape noexcept teardown.
    }
}

::media::Result<MediaNodeProcessResult>
MediaScheduledDatagramSenderNode::failTerminal(::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    emitDiagnostics("failed");
    return ::media::Result<MediaNodeProcessResult>::failure(*m_terminalFailure);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledDatagramSenderNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) return failTerminal(*m_terminalFailure);
    auto planInput = tryPopInputOptional(context, "plan");
    if (!planInput) return failTerminal(planInput.error());
    if (planInput.value()) {
        if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
                planInput.value()->get())) {
            if (control->controlKind() == MediaControlBufferKind::Abort) {
                return failTerminal(::media::ErrorInfo::cancelled(
                    "scheduled datagram sender plan was aborted"));
            }
        } else {
            const auto* plan = dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
                planInput.value()->get());
            if (!plan) return failTerminal(::media::ErrorInfo::invalidArgument(
                "scheduled datagram sender requires a typed runtime plan"));
            auto bound = bindPlan(*plan);
            if (!bound) return failTerminal(bound.error());
        }
    }
    if (!m_generation) {
        MediaChannel* planChannel = context.findInputChannel(nodeId(), "plan");
        if (planChannel && planChannel->aborted()) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "scheduled datagram sender plan input was aborted"));
        }
        if (planChannel && planChannel->closed()) {
            return failTerminal(::media::ErrorInfo::notInitialized(
                "scheduled datagram sender closed before a plan"));
        }
        return processWaiting();
    }
    auto batchInput = tryPopInputOptional(context, "batch");
    if (!batchInput) return failTerminal(batchInput.error());
    if (!batchInput.value()) {
        MediaChannel* batchChannel = context.findInputChannel(nodeId(), "batch");
        if (batchChannel && batchChannel->aborted()) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "scheduled datagram batch input was aborted"));
        }
        if (batchChannel && batchChannel->closed()) {
            return m_generation ? processFinished()
                                : failTerminal(::media::ErrorInfo::notInitialized(
                                      "scheduled datagram sender closed before a plan"));
        }
        return processWaiting();
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            batchInput.value()->get())) {
        if (control->controlKind() == MediaControlBufferKind::Eof) {
            return processFinished();
        }
        if (control->controlKind() == MediaControlBufferKind::Abort) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "scheduled datagram sender received abort"));
        }
        return processProgress();
    }
    const auto* batch = dynamic_cast<const MediaScheduledDatagramBatchBuffer*>(
        batchInput.value()->get());
    if (!batch) return failTerminal(::media::ErrorInfo::invalidArgument(
        "scheduled datagram sender requires a typed batch"));
    auto sent = sendBatch(*batch);
    return sent ? processProgress() : failTerminal(sent.error());
}

void MediaScheduledDatagramSenderNode::closeSender() noexcept
{
    if (m_sink) m_sink->abort();
    m_sink.reset();
}

::media::Status MediaScheduledDatagramSenderNode::stop(
    MediaGraphExecutionContext& context)
{
    std::optional<::media::ErrorInfo> closeFailure;
    if (m_sink) {
        auto closed = m_sink->close();
        if (!closed) closeFailure = closed.error();
    }
    m_sink.reset();
    emitDiagnostics("finished");
    auto base = FFmpegNodeRuntime::stop(context);
    if (closeFailure) return ::media::Status::failure(*closeFailure);
    return base;
}

void MediaScheduledDatagramSenderNode::interrupt(
    MediaGraphExecutionContext&) noexcept
{
    m_wakeup.interrupt();
}

void MediaScheduledDatagramSenderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    interrupt(context);
    emitDiagnostics("aborted");
    closeSender();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "scheduled datagram sender was aborted");
    }
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph

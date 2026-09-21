#include "internal/graph/nodes/output/MediaMpegTsDatagramMaterializerNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/protocol/MediaProtocolTerminalGeneration.h"
#include "internal/graph/protocol/MediaProtocolPurgeInput.h"
#include <array>
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <new>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

} // namespace

MediaMpegTsDatagramMaterializerNode::
MediaMpegTsDatagramMaterializerNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaMpegTsDatagramMaterializerNode"),
      m_authority(std::move(authority))
{
    m_generationState = std::make_shared<MediaProtocolOutputGenerationState>(
        std::string(generationPurgeIdentity()), m_generationSession);
}

::media::Result<std::unique_ptr<MediaMpegTsDatagramMaterializerNode>>
MediaMpegTsDatagramMaterializerNode::create(
    MediaNodeId nodeId,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
{
    using Result = ::media::Result<
        std::unique_ptr<MediaMpegTsDatagramMaterializerNode>>;
    if (!nodeId.isValid() || !authority ||
        !authority->sharedNtpEpoch()) {
        return Result::failure(invalid(
            "MPEG-TS datagram materializer requires id and runtime authority"));
    }
    try {
    auto node = std::unique_ptr<MediaMpegTsDatagramMaterializerNode>(
        new (std::nothrow) MediaMpegTsDatagramMaterializerNode(
            nodeId, std::move(authority)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaMpegTsDatagramMaterializerNode"));
    }
    return Result::success(std::move(node));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Protocol materializer node state"));
    }
}

MediaNodeKind MediaMpegTsDatagramMaterializerNode::staticKind() noexcept
{
    return MediaNodeKind::MpegTsDatagramMaterializer;
}

::media::Status MediaMpegTsDatagramMaterializerNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const auto* protocol = context.findInputChannel(nodeId(), "protocol_plan");
    const auto* transport = context.findInputChannel(nodeId(), "transport_plan");
    const auto* batch = context.findInputChannel(nodeId(), "protocol_batch");
    const auto* wire = context.findOutputChannel(nodeId(), "wire_batch");
    if (context.inputChannels(nodeId()).size() != 3 ||
        context.outputChannels(nodeId()).size() != 1 || !protocol ||
        !transport || !batch || !wire ||
        protocol->binding().payloadKind !=
            MediaPayloadKind::ProjectMpegTsRuntimePlan ||
        transport->binding().payloadKind !=
            MediaPayloadKind::DatagramTransportPlan ||
        batch->binding().payloadKind !=
            MediaPayloadKind::MpegTsProtocolDatagramBatch ||
        wire->binding().payloadKind != MediaPayloadKind::WireDatagramBatch) {
        return ::media::Status::failure(invalid(
            "MPEG-TS datagram materializer requires exact protocol, transport, batch, and wire ports"));
    }
    return ::media::Status::success();
}

::media::Status MediaMpegTsDatagramMaterializerNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    m_completedPurge.reset();
    auto reset = m_generationState->resetLifecycle();
    if (!reset) return reset;
    auto started = m_generationPurge->start(context.sharedNodeWakeup(nodeId()));
    if (!started) return started;
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Status MediaMpegTsDatagramMaterializerNode::tryCreateMaterializer(
    MediaGraphExecutionContext& context)
{
    if (m_generationSession->data->materializer || !m_generationSession->data->protocolPlan || !m_generationSession->data->transportPlan) {
        return ::media::Status::success();
    }
    const auto* protocol =
        dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
            m_generationSession->data->protocolPlan.get());
    const auto* transport =
        dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
            m_generationSession->data->transportPlan.get());
    if (!protocol || !transport ||
        protocol->sessionKey() != m_authority->sessionKey() ||
        protocol->activation().generation !=
            transport->plan().shaping.generation() ||
        protocol->sessionKey().value() !=
            transport->plan().shaping.sessionKey()) {
        return ::media::Status::failure(invalid(
            "MPEG-TS protocol and datagram transport plans differ in session or generation"));
    }
    auto permit = m_generationState->permitActivatedGeneration(*m_authority,
        protocol->activation().generation, protocol->activation().completedTransitionSequence);
    if (!permit) return ::media::Status::failure(
        permit.error().code == ::media::ErrorCode::Cancelled
            ? ::media::ErrorInfo::wouldBlock("MPEG-TS materializer activation awaits generation authority")
            : permit.error());
    const auto& shaping = transport->plan().shaping;
    const auto& output = protocol->outputPlan();
    if (std::holds_alternative<MediaMpegTsUdpOutputPlan>(output.transport)) {
        auto endpointId = transport->endpointId(
            MediaDatagramProtocolEndpointRole::MpegTsUdp);
        const auto* endpoint = endpointId
            ? shaping.endpoint(endpointId.value()) : nullptr;
        if (!endpoint) {
            return ::media::Status::failure(
                endpointId ? invalid("MPEG-TS UDP shaping endpoint is absent")
                           : endpointId.error());
        }
        auto deadline = shaping.wireDeadlinePlan(endpointId.value());
        if (!deadline) {
            return ::media::Status::failure(deadline.error());
        }
        auto created = MediaMpegTsUdpWireDatagramMaterializer::create(
            MediaMpegTsUdpWireDatagramMaterializerConfig{
                shaping.sessionKey(), shaping.serviceScope().scopeId,
                shaping.generation(), endpointId.value(),
                deadline.value(),
                transport->globalSequence(),
                protocol->muxPlan().parameters().packetSize,
                static_cast<std::size_t>(endpoint->maximumDatagramBytes),
                shaping.batch(), context.sharedNodeWakeup(nodeId())});
        if (!created) return ::media::Status::failure(created.error());
        m_generationSession->data->materializer.emplace<MediaMpegTsUdpWireDatagramMaterializer>(
            std::move(created).value());
        return ::media::Status::success();
    }
    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
        &output.transport);
    auto rtpEndpointId = transport->endpointId(
        MediaDatagramProtocolEndpointRole::MpegTsRtp);
    auto rtcpEndpointId = transport->endpointId(
        MediaDatagramProtocolEndpointRole::MpegTsRtcp);
    const auto* endpoint = rtpEndpointId
        ? shaping.endpoint(rtpEndpointId.value()) : nullptr;
    if (!rtp || !rtpEndpointId || !rtcpEndpointId || !endpoint ||
        endpoint->maximumDatagramBytes != rtp->maximumDatagramBytes()) {
        return ::media::Status::failure(invalid(
            "MPEG-TS RTP protocol and endpoint geometry differ"));
    }
    auto rtpDeadline = shaping.wireDeadlinePlan(rtpEndpointId.value());
    auto rtcpDeadline = shaping.wireDeadlinePlan(rtcpEndpointId.value());
    if (!rtpDeadline || !rtcpDeadline) {
        return ::media::Status::failure(
            !rtpDeadline ? rtpDeadline.error() : rtcpDeadline.error());
    }
    auto reportSchedule = MediaRtcpSenderReportSchedule::create(
        protocol->activation().masterRelease, rtp->rtcpReporting(),
        endpoint->maximumResidence, protocol->activation().generation,
        rtp->ssrc());
    if (!reportSchedule) {
        return ::media::Status::failure(reportSchedule.error());
    }
    auto created = MediaMpegTsRtpWireDatagramMaterializer::create(
        MediaMpegTsRtpWireDatagramMaterializerConfig{
            shaping.sessionKey(), shaping.serviceScope().scopeId,
            shaping.generation(), rtpEndpointId.value(),
            rtcpEndpointId.value(), rtpDeadline.value(),
            rtcpDeadline.value(), transport->globalSequence(),
            rtp->payloadType(), rtp->clockRate(), rtp->ssrc(),
            rtp->baseTimestamp(), rtp->initialSequenceNumber(),
            rtp->tsPacketsPerPayload(), rtp->maximumDatagramBytes(),
            shaping.backlog().maximumDatagrams,
            shaping.batch(),
            m_authority->sharedNtpEpoch()->masterAtCapture(),
            *m_authority->sharedNtpEpoch(),
            std::move(reportSchedule).value(), rtp->cname(),
            context.sharedNodeWakeup(nodeId())});
    if (!created) return ::media::Status::failure(created.error());
    m_generationSession->data->materializer.emplace<MediaMpegTsRtpWireDatagramMaterializer>(
        std::move(created).value());
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsDatagramMaterializerNode::process(MediaGraphExecutionContext& context)
{
    if (auto purge = m_generationPurge->pending()) {
        auto activation = m_authority->currentActivation();
        if (!activation || activation.value().generation != purge->oldGeneration)
            return processProgress(::media::Status::failure(activation
                ? invalid("Materializer purge differs from the globally revoked generation") : activation.error()));
        ::media::Status status = ::media::Status::success();
        if (const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
                m_generationSession->data->transportPlan.get())) {
            status = authorizeProtocolPurgeInput(m_generationSession->data->transportPlan,
                m_authority->sessionKey(), *purge, m_completedPurge);
        }
        if (status) {
            static constexpr std::array<const char*, 3> ports{"protocol_plan", "transport_plan", "protocol_batch"};
            for (const auto* port : ports) {
                while (status) {
                    auto input = tryPopInputOptional(context, port);
                    if (!input) { status = ::media::Status::failure(input.error()); break; }
                    if (!input.value()) break;
                    status = authorizeProtocolPurgeInput(*input.value(), m_authority->sessionKey(), *purge, m_completedPurge);
                }
                if (!status) break;
            }
        }
        if (status) {
            cancelPendingOutputTransfer();
            status = m_generationState->purge(*purge);
        }
        if (status) m_completedPurge = *purge;
        auto completed = m_generationPurge->complete(*purge, status);
        if (!completed) return processProgress(completed);
        return processProgress(status);
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaOutputCommitReservation>
MediaMpegTsDatagramMaterializerNode::reserveOutputCommit(const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    const auto* plan = dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(m_generationSession->data->protocolPlan.get());
    std::optional<std::uint64_t> generation = plan ? std::optional(plan->activation().generation) : std::nullopt;
    if (const auto* wire = dynamic_cast<const MediaWireDatagramBatchBuffer*>(buffer.get());
        !wire || !generation || wire->generation() != *generation)
        return Result::failure(invalid("MPEG-TS materializer wire publication differs from active generation"));
    if (!generation) return Result::failure(invalid("Protocol materializer publication lacks its active plan"));
    auto reserved = m_generationState->reserveCommit(*m_authority, *generation);
    if (!reserved) {
        // Revocation precedes delivery of the owner-thread purge request.
        // Keep the bounded output until that request authorizes cancellation.
        return Result::failure(reserved.error().code == ::media::ErrorCode::Cancelled
            ? ::media::ErrorInfo::wouldBlock("Protocol materializer publication awaits generation purge")
            : reserved.error());
    }
    return Result::success(MediaOutputCommitReservation::hold(std::move(reserved).value()));
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsDatagramMaterializerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (!m_generationSession->data->pendingOutputs.empty()) {
        auto emitted = emitOutput(
            context, "wire_batch", m_generationSession->data->pendingOutputs.front());
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    if (m_generationSession->data->terminal) return finishProtocol(context);
    if (!m_generationSession->data->protocolPlan) {
        auto input = tryPopInputOptional(context, "protocol_plan");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (input.value()) {
            const auto* plan = dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(input.value()->get());
            if (!plan || plan->sessionKey() != m_authority->sessionKey())
                return processProgress(::media::Status::failure(invalid("MPEG-TS materializer requires its protocol plan")));
            if (m_completedPurge && plan->activation().generation <= m_completedPurge->oldGeneration)
                return processProgress();
            m_generationSession->data->protocolPlan = std::move(*input.value());
            auto created = tryCreateMaterializer(context);
            return created ? processProgress()
                           : processProgress(std::move(created));
        }
    }
    if (!m_generationSession->data->transportPlan) {
        auto input = tryPopInputOptional(context, "transport_plan");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (input.value()) {
            const auto* plan = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(input.value()->get());
            if (!plan || plan->plan().shaping.sessionKey() != m_authority->sessionKey().value())
                return processProgress(::media::Status::failure(invalid("MPEG-TS materializer requires its transport plan")));
            if (m_completedPurge && plan->plan().shaping.generation() <= m_completedPurge->oldGeneration)
                return processProgress();
            m_generationSession->data->transportPlan = std::move(*input.value());
            auto created = tryCreateMaterializer(context);
            return created ? processProgress()
                           : processProgress(std::move(created));
        }
    }
    if (!m_generationSession->data->materializer) {
        auto created = tryCreateMaterializer(context);
        if (!created) return processProgress(created);
        if (!m_generationSession->data->materializer) return processWaiting();
    }
    if (!m_generationSession->data->pendingProtocolBatch) {
        auto input = tryPopInputOptional(context, "protocol_batch");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (!input.value()) return processWaiting();
        m_generationSession->data->pendingProtocolBatch = std::move(*input.value());
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_generationSession->data->pendingProtocolBatch.get())) {
        auto terminal = classifyProtocolTerminalGeneration(*control, dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(m_generationSession->data->protocolPlan.get())->activation().generation,
            m_authority->streamSet(), m_completedPurge);
        if (!terminal) return processProgress(::media::Status::failure(terminal.error()));
        if (!terminal.value()) {
            m_generationSession->data->pendingProtocolBatch.reset();
            return processProgress();
        }
        m_generationSession->data->terminal = std::move(m_generationSession->data->pendingProtocolBatch);
        return finishProtocol(context);
    }
    auto* batch = dynamic_cast<MediaMpegTsProtocolDatagramBatchBuffer*>(
        m_generationSession->data->pendingProtocolBatch.get());
    if (!batch) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "MPEG-TS datagram materializer requires a protocol batch"));
    }
    if (m_completedPurge && batch->generation() <= m_completedPurge->oldGeneration) {
        m_generationSession->data->pendingProtocolBatch.reset();
        return processProgress();
    }
    auto materializedAt = m_authority->now();
    if (!materializedAt) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            materializedAt.error());
    }
    auto wire = [&]() -> ::media::Result<MediaWireDatagramBatchCollection> {
        auto permit = m_authority->reserveCommit(batch->generation());
        if (!permit) return ::media::Result<MediaWireDatagramBatchCollection>::failure(
            permit.error().code == ::media::ErrorCode::Cancelled
                ? ::media::ErrorInfo::wouldBlock("MPEG-TS materialization awaits purge") : permit.error());
        return std::visit(
        [batch, materializedAt = materializedAt.value()](auto& materializer) {
            return materializer.materializeProtocolBatch(
                *batch, materializedAt);
        },
        *m_generationSession->data->materializer);
    }();
    if (!wire) {
        return processProgress(::media::Status::failure(wire.error()));
    }
    recordMaterialized(wire.value(), materializedAt.value());
    try {
        for (auto& partition : wire.value()) {
            m_generationSession->data->pendingOutputs.push_back(std::move(partition));
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MPEG-TS wire output partitions"));
    }
    if (m_generationSession->data->pendingOutputs.empty()) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "MPEG-TS wire materializer emitted no batch partitions"));
    }
    auto emitted = emitOutput(
        context, "wire_batch", m_generationSession->data->pendingOutputs.front());
    return emitted ? processProgress() : processProgress(std::move(emitted));
}

::media::Status MediaMpegTsDatagramMaterializerNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    if (m_generationSession->data->pendingOutputs.empty() || buffer != m_generationSession->data->pendingOutputs.front()) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "MPEG-TS wire batch commit differs from pending output"));
    }
    m_generationSession->data->pendingOutputs.pop_front();
    if (m_generationSession->data->pendingOutputs.empty()) {
        const auto* protocol = dynamic_cast<const MediaMpegTsProtocolDatagramBatchBuffer*>(
            m_generationSession->data->pendingProtocolBatch.get());
        if (!protocol || protocol->datagrams().empty()) m_generationSession->data->pendingProtocolBatch.reset();
    }
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsDatagramMaterializerNode::finishProtocol(MediaGraphExecutionContext& context)
{
    const auto generation = dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(m_generationSession->data->protocolPlan.get())->activation().generation;
    const auto* control = dynamic_cast<const MediaControlBuffer*>(m_generationSession->data->terminal.get());
    if (!control) return processProgress(::media::Status::failure(invalid("Protocol terminal buffer is absent")));
    auto terminal = classifyProtocolTerminalGeneration(*control, generation, m_authority->streamSet(), m_completedPurge);
    if (!terminal) return processProgress(::media::Status::failure(terminal.error()));
    if (!terminal.value()) { m_generationSession->data->terminal.reset(); return processProgress(); }
    if (!m_generationSession->data->terminalReportPrepared) {
        auto publication = m_authority->reserveCommit(generation);
        if (!publication) return publication.error().code == ::media::ErrorCode::Cancelled
            ? processWaiting() : processProgress(::media::Status::failure(publication.error()));
        if (auto* rtp = std::get_if<MediaMpegTsRtpWireDatagramMaterializer>(&*m_generationSession->data->materializer)) {
            auto now = m_authority->now();
            if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
            auto report = rtp->materializeTerminalReport(now.value(), now.value(), now.value());
            if (!report) return processProgress(::media::Status::failure(report.error()));
            if (report.value()) m_generationSession->data->pendingOutputs.push_back(std::move(report).value());
        }
        m_generationSession->data->terminalReportPrepared = true;
    }
    if (!m_generationSession->data->pendingOutputs.empty())
        return processProgress(emitOutput(context, "wire_batch", m_generationSession->data->pendingOutputs.front()));
    auto* wire = context.findOutputChannel(nodeId(), "wire_batch");
    if (!wire) return ::media::Result<MediaNodeProcessResult>::failure(invalid(
        "protocol termination requires its planned wire output"));
    auto publication = m_authority->reserveCommit(generation);
    if (!publication) return publication.error().code == ::media::ErrorCode::Cancelled
        ? processWaiting() : processProgress(::media::Status::failure(publication.error()));
    return processFinished(wire->closeWithTerminal(m_generationSession->data->terminal));
}

::media::Status MediaMpegTsDatagramMaterializerNode::stop(
    MediaGraphExecutionContext& context)
{
    emitDiagnostics("stopped");
    m_generationPurge->stop();
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaMpegTsDatagramMaterializerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    emitDiagnostics("aborted");
    m_generationPurge->stop();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaMpegTsDatagramMaterializerNode::recordMaterialized(
    const MediaWireDatagramBatchCollection& batches,
    MediaRunningTime materializedAt) noexcept
{
    for (const auto& batch : batches) {
        if (!batch) continue;
        for (const auto& datagram : batch->datagrams()) {
            const auto afterRelease = materializedAt.checkedSubtract(
                datagram.canonicalRelease());
            if (!afterRelease ||
                afterRelease.value().nanoseconds() <=
                    m_maximumMaterializedAfterReleaseNanoseconds) {
                continue;
            }
            m_maximumMaterializedAfterReleaseNanoseconds =
                afterRelease.value().nanoseconds();
            m_worstMaterializedAtNanoseconds = materializedAt.nanoseconds();
            m_worstCanonicalReleaseNanoseconds =
                datagram.canonicalRelease().nanoseconds();
            m_worstCanonicalDeadlineNanoseconds =
                datagram.canonicalDeadline().nanoseconds();
            m_worstGlobalSequence = datagram.globalSequence();
        }
    }
}

void MediaMpegTsDatagramMaterializerNode::emitDiagnostics(
    const char* stage) noexcept
{
    if (m_diagnosticsEmitted) return;
    m_diagnosticsEmitted = true;
    try {
        std::ostringstream out;
        out << "mpegts_wire_materializer stage=" << stage
            << " maximum_materialized_after_release_ns="
            << m_maximumMaterializedAfterReleaseNanoseconds
            << " worst_materialized_at_ns="
            << m_worstMaterializedAtNanoseconds
            << " worst_release_ns="
            << m_worstCanonicalReleaseNanoseconds
            << " worst_deadline_ns="
            << m_worstCanonicalDeadlineNanoseconds
            << " worst_global_sequence=" << m_worstGlobalSequence;
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::Summary,
            MediaGraphDiagnosticPhase::RuntimeNode,
            out.str());
    } catch (...) {
    }
}

void MediaMpegTsDatagramMaterializerNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_generationSession->data->pendingOutputs.clear();
    m_generationSession->data->pendingProtocolBatch.reset();
    m_generationSession->data->terminal.reset();
    m_generationSession->data->terminalReportPrepared = false;
    m_generationSession->data->materializer.reset();
    m_generationSession->data->transportPlan.reset();
    m_generationSession->data->protocolPlan.reset();
    m_maximumMaterializedAfterReleaseNanoseconds = 0;
    m_worstMaterializedAtNanoseconds = 0;
    m_worstCanonicalReleaseNanoseconds = 0;
    m_worstCanonicalDeadlineNanoseconds = 0;
    m_worstGlobalSequence = 0;
    m_diagnosticsEmitted = false;
}

} // namespace media::ffmpeg::graph

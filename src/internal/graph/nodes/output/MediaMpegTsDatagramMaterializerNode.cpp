#include "internal/graph/nodes/output/MediaMpegTsDatagramMaterializerNode.h"

#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <new>
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
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority) noexcept
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaMpegTsDatagramMaterializerNode"),
      m_authority(std::move(authority))
{
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
    auto node = std::unique_ptr<MediaMpegTsDatagramMaterializerNode>(
        new (std::nothrow) MediaMpegTsDatagramMaterializerNode(
            nodeId, std::move(authority)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaMpegTsDatagramMaterializerNode"));
    }
    return Result::success(std::move(node));
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
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Status MediaMpegTsDatagramMaterializerNode::tryCreateMaterializer()
{
    if (m_materializer || !m_protocolPlan || !m_transportPlan) {
        return ::media::Status::success();
    }
    const auto* protocol =
        dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
            m_protocolPlan.get());
    const auto* transport =
        dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
            m_transportPlan.get());
    if (!protocol || !transport ||
        protocol->sessionKey() != m_authority->sessionKey() ||
        protocol->activation().generation !=
            transport->plan().shaping.generation() ||
        protocol->sessionKey().value() !=
            transport->plan().shaping.sessionKey()) {
        return ::media::Status::failure(invalid(
            "MPEG-TS protocol and datagram transport plans differ in session or generation"));
    }
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
                shaping.batch()});
        if (!created) return ::media::Status::failure(created.error());
        m_materializer.emplace<MediaMpegTsUdpWireDatagramMaterializer>(
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
    auto firstReport = protocol->activation().masterRelease.checkedAdd(
        rtp->senderReportInterval());
    if (!firstReport) return ::media::Status::failure(firstReport.error());
    auto reportSchedule = MediaRtcpSenderReportSchedule::create(
        firstReport.value(), rtp->senderReportInterval(),
        endpoint->maximumResidence, protocol->activation().generation);
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
            std::move(reportSchedule).value(), rtp->cname()});
    if (!created) return ::media::Status::failure(created.error());
    m_materializer.emplace<MediaMpegTsRtpWireDatagramMaterializer>(
        std::move(created).value());
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsDatagramMaterializerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (!m_pendingOutputs.empty()) {
        auto emitted = emitOutput(
            context, "wire_batch", m_pendingOutputs.front());
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    if (!m_protocolPlan) {
        auto input = tryPopInputOptional(context, "protocol_plan");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (input.value()) {
            m_protocolPlan = std::move(*input.value());
            auto created = tryCreateMaterializer();
            return created ? processProgress()
                           : processProgress(std::move(created));
        }
    }
    if (!m_transportPlan) {
        auto input = tryPopInputOptional(context, "transport_plan");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (input.value()) {
            m_transportPlan = std::move(*input.value());
            auto created = tryCreateMaterializer();
            return created ? processProgress()
                           : processProgress(std::move(created));
        }
    }
    if (!m_materializer) return processWaiting();
    if (!m_pendingProtocolBatch) {
        auto input = tryPopInputOptional(context, "protocol_batch");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (!input.value()) return processWaiting();
        m_pendingProtocolBatch = std::move(*input.value());
    }
    auto* batch = dynamic_cast<MediaMpegTsProtocolDatagramBatchBuffer*>(
        m_pendingProtocolBatch.get());
    if (!batch) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "MPEG-TS datagram materializer requires a protocol batch"));
    }
    auto materializedAt = m_authority->now();
    if (!materializedAt) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            materializedAt.error());
    }
    auto wire = std::visit(
        [batch, materializedAt = materializedAt.value()](auto& materializer) {
            return materializer.materializeProtocolBatch(
                *batch, materializedAt);
        },
        *m_materializer);
    if (!wire) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            wire.error());
    }
    try {
        for (auto& partition : wire.value()) {
            m_pendingOutputs.push_back(std::move(partition));
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MPEG-TS wire output partitions"));
    }
    if (m_pendingOutputs.empty()) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "MPEG-TS wire materializer emitted no batch partitions"));
    }
    auto emitted = emitOutput(
        context, "wire_batch", m_pendingOutputs.front());
    return emitted ? processProgress() : processProgress(std::move(emitted));
}

::media::Status MediaMpegTsDatagramMaterializerNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    if (m_pendingOutputs.empty() || buffer != m_pendingOutputs.front()) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "MPEG-TS wire batch commit differs from pending output"));
    }
    m_pendingOutputs.pop_front();
    if (m_pendingOutputs.empty()) m_pendingProtocolBatch.reset();
    return ::media::Status::success();
}

::media::Status MediaMpegTsDatagramMaterializerNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaMpegTsDatagramMaterializerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaMpegTsDatagramMaterializerNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_pendingOutputs.clear();
    m_materializer.reset();
    m_transportPlan.reset();
    m_protocolPlan.reset();
}

} // namespace media::ffmpeg::graph

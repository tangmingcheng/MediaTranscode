#include "internal/graph/nodes/output/MediaRtpDatagramMaterializerNode.h"

#include "internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

std::pair<MediaDatagramProtocolEndpointRole,
          MediaDatagramProtocolEndpointRole>
endpointRoles(MediaScheduledStream stream) noexcept
{
    return stream == MediaScheduledStream::Video
        ? std::pair{MediaDatagramProtocolEndpointRole::VideoRtp,
                    MediaDatagramProtocolEndpointRole::VideoRtcp}
        : std::pair{MediaDatagramProtocolEndpointRole::AudioRtp,
                    MediaDatagramProtocolEndpointRole::AudioRtcp};
}

} // namespace

MediaRtpDatagramMaterializerNode::MediaRtpDatagramMaterializerNode(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSessionKey,
    MediaScheduledRtpOutputPlan outputPlan,
    MediaSeparateRtpSdpRuntimePlan sdpPlan,
    MediaRtpDatagramMaterializerNodeDependencies dependencies)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaRtpDatagramMaterializerNode"),
      m_plannedSessionKey(std::move(plannedSessionKey)),
      m_outputPlan(std::move(outputPlan)),
      m_sdpPlan(std::move(sdpPlan)),
      m_dependencies(std::move(dependencies))
{
}

::media::Result<std::unique_ptr<MediaRtpDatagramMaterializerNode>>
MediaRtpDatagramMaterializerNode::create(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSessionKey,
    MediaScheduledRtpOutputPlan outputPlan,
    MediaSeparateRtpSdpRuntimePlan sdpPlan,
    MediaRtpDatagramMaterializerNodeDependencies dependencies)
{
    using Result = ::media::Result<
        std::unique_ptr<MediaRtpDatagramMaterializerNode>>;
    if (!nodeId.isValid() || !plannedSessionKey.valid() ||
        !dependencies.authority || !dependencies.packetizerFactory ||
        dependencies.authority->sessionKey() != plannedSessionKey ||
        !dependencies.authority->sharedNtpEpoch()) {
        return Result::failure(invalid(
            "RTP datagram materializer requires session, authority, and packetizer factory"));
    }
    auto node = std::unique_ptr<MediaRtpDatagramMaterializerNode>(
        new (std::nothrow) MediaRtpDatagramMaterializerNode(
            nodeId, std::move(plannedSessionKey), std::move(outputPlan),
            std::move(sdpPlan), std::move(dependencies)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaRtpDatagramMaterializerNode"));
    }
    return Result::success(std::move(node));
}

MediaNodeKind MediaRtpDatagramMaterializerNode::staticKind() noexcept
{
    return MediaNodeKind::RtpDatagramMaterializer;
}

::media::Status MediaRtpDatagramMaterializerNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const auto* activation = context.findInputChannel(nodeId(), "activation");
    const auto* codec = context.findInputChannel(nodeId(), "codec");
    const auto* transport = context.findInputChannel(nodeId(), "transport_plan");
    const auto* scheduled = context.findInputChannel(nodeId(), "scheduled");
    const auto* description = context.findOutputChannel(nodeId(), "description");
    const auto* wire = context.findOutputChannel(nodeId(), "wire_batch");
    if (context.inputChannels(nodeId()).size() != 4 ||
        context.outputChannels(nodeId()).size() != 2 || !activation ||
        !codec || !transport || !scheduled || !description || !wire ||
        transport->binding().payloadKind !=
            MediaPayloadKind::DatagramTransportPlan ||
        wire->binding().payloadKind != MediaPayloadKind::WireDatagramBatch) {
        return ::media::Status::failure(invalid(
            "RTP datagram materializer requires activation, codec, transport, scheduled, description, and wire ports"));
    }
    return ::media::Status::success();
}

::media::Status MediaRtpDatagramMaterializerNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto valid = validatePorts(context);
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Result<bool> MediaRtpDatagramMaterializerNode::acquireBindings(
    MediaGraphExecutionContext& context)
{
    bool progressed = false;
    auto acquire = [&](const char* port, MediaBufferRef& target)
        -> ::media::Status {
        if (target) return ::media::Status::success();
        auto input = tryPopInputOptional(context, port);
        if (!input) return ::media::Status::failure(input.error());
        if (input.value()) {
            target = std::move(*input.value());
            progressed = true;
        }
        return ::media::Status::success();
    };
    if (auto status = acquire("activation", m_activation); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (m_activation && !m_activationFacts) {
        auto activation = m_dependencies.authority->validateActivation(
            m_activation);
        if (!activation) return ::media::Result<bool>::failure(activation.error());
        m_activationFacts = activation.value();
    }
    if (auto status = acquire("codec", m_codec); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (auto status = acquire("transport_plan", m_transportPlan); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(progressed);
}

::media::Status MediaRtpDatagramMaterializerNode::openProtocol(
    const AVPacket* configurationPacket)
{
    if (m_packetizer || !m_activationFacts || !m_codec || !m_transportPlan) {
        return m_packetizer
            ? ::media::Status::success()
            : ::media::Status::failure(::media::ErrorInfo::notInitialized(
                  "RTP protocol materializer bindings are incomplete"));
    }
    const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(
        m_codec.get());
    const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
        m_transportPlan.get());
    if (!codec || !codec->context() || !transport ||
        transport->plan().shaping.sessionKey() != m_plannedSessionKey.value() ||
        transport->plan().shaping.generation() != m_activationFacts->generation) {
        return ::media::Status::failure(invalid(
            "RTP codec and datagram transport bindings differ from activation"));
    }
    auto materialized = MediaScheduledRtpSenderMaterializer::materialize(
        m_outputPlan, m_sdpPlan, *codec->context(), configurationPacket,
        *m_dependencies.authority->sharedNtpEpoch(), *m_activationFacts);
    if (!materialized) return ::media::Status::failure(materialized.error());
    m_pendingOutput = materialized.value().releaseDescription();
    m_pendingOutputKind = PendingOutputKind::Description;
    auto senderConfig = materialized.value().releaseSenderConfig();
    auto packetizer = m_dependencies.packetizerFactory->create(
        senderConfig.releaseStreamConfig(),
        [this](std::span<const std::uint8_t> bytes,
               std::size_t payloadOctets) {
            return collectPacketizedDatagram(bytes, payloadOctets);
        });
    if (!packetizer) return ::media::Status::failure(packetizer.error());
    auto opened = packetizer.value()->open();
    if (!opened) return opened;
    m_packetizer = std::move(packetizer).value();
    return ::media::Status::success();
}

::media::Status MediaRtpDatagramMaterializerNode::collectPacketizedDatagram(
    std::span<const std::uint8_t> bytes,
    std::size_t payloadOctets)
{
    if (bytes.size() < 12 || payloadOctets == 0) {
        return ::media::Status::failure(invalid(
            "RTP packetizer callback returned an invalid datagram"));
    }
    try {
        m_packetizedBytes.emplace_back(bytes.begin(), bytes.end());
        m_packetizedPayloadOctets.push_back(payloadOctets);
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "RTP packetized access-unit batch"));
    }
    return ::media::Status::success();
}

::media::Status MediaRtpDatagramMaterializerNode::createWireMaterializer(
    std::uint16_t initialSequence)
{
    if (m_wireMaterializer) return ::media::Status::success();
    const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
        m_transportPlan.get());
    if (!transport || !m_activationFacts) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "RTP wire materializer has no activated transport plan"));
    }
    const auto [rtpRole, rtcpRole] = endpointRoles(m_outputPlan.stream);
    auto rtpEndpoint = transport->endpointId(rtpRole);
    auto rtcpEndpoint = transport->endpointId(rtcpRole);
    const auto* endpoint = rtpEndpoint
        ? transport->plan().shaping.endpoint(rtpEndpoint.value()) : nullptr;
    if (!rtpEndpoint || !rtcpEndpoint || !endpoint ||
        endpoint->maximumDatagramBytes !=
            m_outputPlan.packetization.maximumDatagramBytes()) {
        return ::media::Status::failure(invalid(
            "RTP protocol and deployment endpoint geometry differ"));
    }
    auto rtpDeadline = transport->plan().shaping.wireDeadlinePlan(
        rtpEndpoint.value());
    auto rtcpDeadline = transport->plan().shaping.wireDeadlinePlan(
        rtcpEndpoint.value());
    if (!rtpDeadline || !rtcpDeadline) {
        return ::media::Status::failure(
            !rtpDeadline ? rtpDeadline.error() : rtcpDeadline.error());
    }
    auto identity = MediaRtpDatagramRewriteIdentity::create(
        m_outputPlan.packetization.payloadType(), m_outputPlan.ssrc);
    if (!identity) return ::media::Status::failure(identity.error());
    auto mapper = MediaRtpOutputClockMapper::create(
        m_outputPlan.clockRate, m_outputPlan.baseTimestamp,
        m_activationFacts->masterRelease);
    if (!mapper) return ::media::Status::failure(mapper.error());
    auto firstReport = m_activationFacts->masterRelease.checkedAdd(
        m_outputPlan.senderReportInterval);
    if (!firstReport) return ::media::Status::failure(firstReport.error());
    auto schedule = MediaRtcpSenderReportSchedule::create(
        firstReport.value(), m_outputPlan.senderReportInterval,
        endpoint->maximumResidence, m_activationFacts->generation);
    if (!schedule) return ::media::Status::failure(schedule.error());
    auto created = MediaRtpWireDatagramMaterializer::create(
        MediaRtpWireDatagramMaterializerConfig{
            transport->plan().shaping.sessionKey(),
            transport->plan().shaping.serviceScope().scopeId,
            m_activationFacts->generation, rtpEndpoint.value(),
            rtcpEndpoint.value(), rtpDeadline.value(),
            rtcpDeadline.value(), transport->globalSequence(),
            identity.value(), mapper.value(),
            *m_dependencies.authority->sharedNtpEpoch(),
            std::move(schedule).value(), m_outputPlan.cname,
            initialSequence, 0, 0,
            m_outputPlan.packetization.maximumDatagramBytes(),
            transport->plan().shaping.backlog().maximumDatagrams});
    if (!created) return ::media::Status::failure(created.error());
    m_wireMaterializer = std::move(created).value();
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaRtpDatagramMaterializerNode::processAccessUnit(
    MediaGraphExecutionContext& context)
{
    if (!m_pendingAccessUnit && m_stagedConfigurationAccessUnit) {
        m_pendingAccessUnit = std::move(m_stagedConfigurationAccessUnit);
    }
    if (!m_pendingAccessUnit) {
        auto popped = tryPopInputOptional(context, "scheduled");
        if (!popped) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                popped.error());
        }
        if (!popped.value()) return processWaiting();
        m_pendingAccessUnit = std::move(*popped.value());
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_pendingAccessUnit.get())) {
        if (control->controlKind() == MediaControlBufferKind::Eof) {
            return processFinished();
        }
        return control->controlKind() == MediaControlBufferKind::Abort
            ? ::media::Result<MediaNodeProcessResult>::failure(
                  ::media::ErrorInfo::cancelled(
                      "RTP datagram materializer received abort"))
            : processProgress();
    }
    const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(
        m_pendingAccessUnit.get());
    const AVPacket* packet = scheduled
        ? FFmpegPacketView::packet(scheduled->media()) : nullptr;
    if (!scheduled || !packet || scheduled->stream() != m_outputPlan.stream ||
        !m_activationFacts ||
        scheduled->generation() != m_activationFacts->generation ||
        scheduled->dispatchOnMaster() < scheduled->emitOnMaster()) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "RTP protocol materializer rejects access-unit generation, stream, packet, or timing"));
    }
    if (m_packetizedBytes.empty()) {
        m_packetizedPayloadOctets.clear();
        auto mapper = MediaRtpOutputClockMapper::create(
            m_outputPlan.clockRate, m_outputPlan.baseTimestamp,
            m_activationFacts->masterRelease);
        if (!mapper) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                mapper.error());
        }
        auto timestamp = mapper.value().map(
            scheduled->presentationOnMaster());
        if (!timestamp) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                timestamp.error());
        }
        auto packetized = m_packetizer->writeAccessUnit(
            *packet, timestamp.value());
        if (!packetized || m_packetizedBytes.empty() ||
            m_packetizedBytes.size() != m_packetizedPayloadOctets.size()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                packetized
                    ? invalid("RTP packetizer emitted no complete AU batch")
                    : packetized.error());
        }
        const auto initialSequence = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(m_packetizedBytes.front()[2]) << 8) |
            static_cast<std::uint16_t>(m_packetizedBytes.front()[3]));
        auto wireReady = createWireMaterializer(initialSequence);
        if (!wireReady) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                wireReady.error());
        }
    }
    std::vector<MediaPacketizedRtpDatagramView> views;
    try {
        views.reserve(m_packetizedBytes.size());
        for (std::size_t index = 0; index < m_packetizedBytes.size(); ++index) {
            views.push_back(MediaPacketizedRtpDatagramView{
                m_packetizedBytes[index], m_packetizedPayloadOctets[index],
                scheduled->presentationOnMaster(),
                scheduled->emitOnMaster()});
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed("RTP wire AU views"));
    }
    auto wire = m_wireMaterializer->materializeBatch(views);
    if (!wire) {
        return ::media::Result<MediaNodeProcessResult>::failure(wire.error());
    }
    m_pendingAccessUnit.reset();
    m_packetizedPayloadOctets.clear();
    m_packetizedBytes.clear();
    m_pendingOutput = std::move(wire).value();
    m_pendingOutputKind = PendingOutputKind::Wire;
    auto emitted = emitOutput(context, "wire_batch", m_pendingOutput);
    return emitted ? processProgress() : processProgress(std::move(emitted));
}

::media::Result<MediaNodeProcessResult>
MediaRtpDatagramMaterializerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_pendingOutput) {
        const char* port = m_pendingOutputKind == PendingOutputKind::Description
            ? "description" : "wire_batch";
        auto emitted = emitOutput(context, port, m_pendingOutput);
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    auto bindings = acquireBindings(context);
    if (!bindings) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            bindings.error());
    }
    if (!m_activationFacts || !m_codec || !m_transportPlan) {
        return bindings.value() ? processProgress() : processWaiting();
    }
    if (!m_packetizer) {
        const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(
            m_codec.get());
        const bool needsAccessUnit = codec && codec->context() &&
            (m_outputPlan.packetization.packetizationMode() ==
                 MediaScheduledRtpPacketizationMode::H264AnnexB ||
             m_outputPlan.packetization.packetizationMode() ==
                 MediaScheduledRtpPacketizationMode::HevcAnnexB) &&
            (!codec->context()->extradata ||
             codec->context()->extradata_size <= 0);
        const AVPacket* configurationPacket = nullptr;
        if (needsAccessUnit) {
            auto input = tryPopInputOptional(context, "scheduled");
            if (!input) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    input.error());
            }
            if (!input.value()) return processWaiting();
            m_stagedConfigurationAccessUnit = std::move(*input.value());
            const auto* scheduled =
                dynamic_cast<const MediaScheduledAccessUnit*>(
                    m_stagedConfigurationAccessUnit.get());
            configurationPacket = scheduled
                ? FFmpegPacketView::packet(scheduled->media()) : nullptr;
            if (!configurationPacket) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    invalid("RTP configuration requires first access unit"));
            }
        }
        auto opened = openProtocol(configurationPacket);
        if (!opened) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                opened.error());
        }
        auto emitted = emitOutput(context, "description", m_pendingOutput);
        return emitted ? processProgress() : processProgress(std::move(emitted));
    }
    if (!m_descriptionEmitted) return processWaiting();
    return processAccessUnit(context);
}

::media::Status MediaRtpDatagramMaterializerNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    if (!m_pendingOutput || buffer != m_pendingOutput) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "RTP materializer output commit differs from pending output"));
    }
    if (m_pendingOutputKind == PendingOutputKind::Description) {
        m_descriptionEmitted = true;
    }
    m_pendingOutput.reset();
    m_pendingOutputKind = PendingOutputKind::None;
    return ::media::Status::success();
}

::media::Status MediaRtpDatagramMaterializerNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaRtpDatagramMaterializerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaRtpDatagramMaterializerNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_packetizedPayloadOctets.clear();
    m_packetizedBytes.clear();
    m_wireMaterializer.reset();
    m_packetizer.reset();
    m_pendingOutput.reset();
    m_pendingOutputKind = PendingOutputKind::None;
    m_descriptionEmitted = false;
    m_stagedConfigurationAccessUnit.reset();
    m_pendingAccessUnit.reset();
    m_transportPlan.reset();
    m_codec.reset();
    m_activationFacts.reset();
    m_activation.reset();
}

} // namespace media::ffmpeg::graph

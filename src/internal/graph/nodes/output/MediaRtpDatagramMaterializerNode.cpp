#include "internal/graph/nodes/output/MediaRtpDatagramMaterializerNode.h"

#include "internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/protocol/MediaProtocolTerminalGeneration.h"
#include "internal/graph/protocol/MediaProtocolPurgeInput.h"
#include <array>
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
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
    m_generationState = std::make_shared<MediaProtocolOutputGenerationState>(
        std::string(generationPurgeIdentity()), m_generationSession);
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
    try {
    auto node = std::unique_ptr<MediaRtpDatagramMaterializerNode>(
        new (std::nothrow) MediaRtpDatagramMaterializerNode(
            nodeId, std::move(plannedSessionKey), std::move(outputPlan),
            std::move(sdpPlan), std::move(dependencies)));
    if (!node) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaRtpDatagramMaterializerNode"));
    }
    return Result::success(std::move(node));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Protocol materializer node state"));
    }
}

MediaNodeKind MediaRtpDatagramMaterializerNode::staticKind() noexcept
{
    return MediaNodeKind::RtpDatagramMaterializer;
}

std::string_view MediaRtpDatagramMaterializerNode::generationPurgeIdentity() const noexcept
{
    return m_outputPlan.stream == MediaScheduledStream::Video
        ? "rtp_video_materializer_generation_state" : "rtp_audio_materializer_generation_state";
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
    m_completedPurge.reset();
    auto reset = m_generationState->resetLifecycle();
    if (!reset) return reset;
    auto started = m_generationPurge->start(context.sharedNodeWakeup(nodeId()));
    if (!started) return started;
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
            progressed = true;
            if (m_completedPurge) {
                if (const auto* activation = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(input.value()->get());
                    activation && activation->groupKey().value() == m_plannedSessionKey.value() &&
                    activation->epoch().generation <= m_completedPurge->oldGeneration)
                    return ::media::Status::success();
                if (const auto* plan = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(input.value()->get());
                    plan && plan->plan().shaping.sessionKey() == m_plannedSessionKey.value() &&
                    plan->plan().shaping.generation() <= m_completedPurge->oldGeneration)
                    return ::media::Status::success();
            }
            target = std::move(*input.value());
        }
        return ::media::Status::success();
    };
    if (auto status = acquire("activation", m_generationSession->data->activation); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (m_generationSession->data->activation && !m_generationSession->data->activationFacts) {
        auto activation = m_dependencies.authority->validateActivation(
            m_generationSession->data->activation);
        if (!activation) return ::media::Result<bool>::failure(activation.error());
        m_generationSession->data->activationFacts = activation.value();
    }
    if (auto status = acquire("codec", m_codec); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (const auto* contextMetadata = dynamic_cast<const FFmpegCodecContextBuffer*>(m_codec.get())) {
        if (!contextMetadata->context()) return ::media::Result<bool>::failure(invalid("RTP codec context metadata is empty"));
        auto snapshot = FFmpegCodecParametersMaterializer::snapshot(*contextMetadata->context());
        if (!snapshot) return ::media::Result<bool>::failure(snapshot.error());
        m_codec = std::move(snapshot).value();
    }
    if (auto status = acquire("transport_plan", m_generationSession->data->transportPlan); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(progressed);
}

::media::Status MediaRtpDatagramMaterializerNode::openProtocol(
    const AVPacket* configurationPacket)
{
    if (m_generationSession->data->packetizer || !m_generationSession->data->activationFacts || !m_codec || !m_generationSession->data->transportPlan) {
        return m_generationSession->data->packetizer
            ? ::media::Status::success()
            : ::media::Status::failure(::media::ErrorInfo::notInitialized(
                  "RTP protocol materializer bindings are incomplete"));
    }
    const auto* codec = dynamic_cast<const FFmpegCodecParametersBuffer*>(
        m_codec.get());
    const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
        m_generationSession->data->transportPlan.get());
    if (!codec || !codec->parameters() || !transport ||
        transport->plan().shaping.sessionKey() != m_plannedSessionKey.value() ||
        transport->plan().shaping.generation() != m_generationSession->data->activationFacts->generation) {
        return ::media::Status::failure(invalid(
            "RTP codec and datagram transport bindings differ from activation"));
    }
    auto permit = m_generationState->permitActivatedGeneration(*m_dependencies.authority,
        m_generationSession->data->activationFacts->generation,
        m_generationSession->data->activationFacts->completedTransitionSequence);
    if (!permit) return ::media::Status::failure(
        permit.error().code == ::media::ErrorCode::Cancelled
            ? ::media::ErrorInfo::wouldBlock("RTP materializer activation awaits generation authority")
            : permit.error());
    auto materialized = MediaScheduledRtpSenderMaterializer::materialize(
        m_outputPlan, m_sdpPlan, *codec->parameters(), m_codec->timeDescriptor().timeBase, configurationPacket,
        *m_dependencies.authority->sharedNtpEpoch(), *m_generationSession->data->activationFacts);
    if (!materialized) return ::media::Status::failure(materialized.error());
    m_generationSession->data->pendingDescription = materialized.value().releaseDescription();
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
    m_generationSession->data->packetizer = std::move(packetizer).value();
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
        m_generationSession->data->packetizedBytes.emplace_back(bytes.begin(), bytes.end());
        m_generationSession->data->packetizedPayloadOctets.push_back(payloadOctets);
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "RTP packetized access-unit batch"));
    }
    return ::media::Status::success();
}

::media::Status MediaRtpDatagramMaterializerNode::createWireMaterializer(
    MediaGraphExecutionContext& context,
    std::uint16_t initialSequence)
{
    if (m_generationSession->data->wireMaterializer) return ::media::Status::success();
    const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
        m_generationSession->data->transportPlan.get());
    if (!transport || !m_generationSession->data->activationFacts) {
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
        m_generationSession->data->activationFacts->masterRelease);
    if (!mapper) return ::media::Status::failure(mapper.error());
    auto schedule = MediaRtcpSenderReportSchedule::create(
        m_generationSession->data->activationFacts->masterRelease, m_outputPlan.rtcpReporting,
        endpoint->maximumResidence, m_generationSession->data->activationFacts->generation,
        m_outputPlan.ssrc);
    if (!schedule) return ::media::Status::failure(schedule.error());
    auto created = MediaRtpWireDatagramMaterializer::create(
        MediaRtpWireDatagramMaterializerConfig{
            transport->plan().shaping.sessionKey(),
            transport->plan().shaping.serviceScope().scopeId,
            m_generationSession->data->activationFacts->generation, rtpEndpoint.value(),
            rtcpEndpoint.value(), rtpDeadline.value(),
            rtcpDeadline.value(), transport->globalSequence(),
            identity.value(), mapper.value(),
            *m_dependencies.authority->sharedNtpEpoch(),
            std::move(schedule).value(), m_outputPlan.cname,
            initialSequence, 0, 0,
            m_outputPlan.packetization.maximumDatagramBytes(),
            transport->plan().shaping.backlog().maximumDatagrams,
            transport->plan().shaping.batch(),
            context.sharedNodeWakeup(nodeId())});
    if (!created) return ::media::Status::failure(created.error());
    m_generationSession->data->wireMaterializer = std::move(created).value();
    return ::media::Status::success();
}

::media::Result<bool> MediaRtpDatagramMaterializerNode::discardPurgedScheduled(MediaBufferRef& buffer)
{
    if (!m_completedPurge || !buffer) return ::media::Result<bool>::success(false);
    std::optional<std::uint64_t> generation;
    if (const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(buffer.get()))
        generation = scheduled->generation();
    else if (const auto* control = dynamic_cast<const MediaControlBuffer*>(buffer.get()))
        generation = control->generation();
    if (!generation || *generation == 0) return ::media::Result<bool>::failure(
        invalid("RTP materializer recovery requires generation-tagged scheduled input"));
    if (*generation > m_completedPurge->oldGeneration) return ::media::Result<bool>::success(false);
    buffer.reset();
    return ::media::Result<bool>::success(true);
}

::media::Result<MediaNodeProcessResult>
MediaRtpDatagramMaterializerNode::processAccessUnit(
    MediaGraphExecutionContext& context)
{
    if (!m_generationSession->data->pendingAccessUnit && m_generationSession->data->stagedConfigurationAccessUnit) {
        m_generationSession->data->pendingAccessUnit = std::move(m_generationSession->data->stagedConfigurationAccessUnit);
    }
    if (!m_generationSession->data->pendingAccessUnit) {
        auto popped = tryPopInputOptional(context, "scheduled");
        if (!popped) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                popped.error());
        }
        if (!popped.value()) return processWaiting();
        m_generationSession->data->pendingAccessUnit = std::move(*popped.value());
    }
    auto discarded = discardPurgedScheduled(m_generationSession->data->pendingAccessUnit);
    if (!discarded) return processProgress(::media::Status::failure(discarded.error()));
    if (discarded.value()) return processProgress();
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_generationSession->data->pendingAccessUnit.get())) {
        auto terminal = classifyProtocolTerminalGeneration(*control, m_generationSession->data->activationFacts->generation,
            m_dependencies.authority->streamSet(), m_completedPurge);
        if (!terminal) return processProgress(::media::Status::failure(terminal.error()));
        if (!terminal.value()) {
            m_generationSession->data->pendingAccessUnit.reset();
            return processProgress();
        }
        m_generationSession->data->terminal = std::move(m_generationSession->data->pendingAccessUnit);
        return finishProtocol(context);
    }

    const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(
        m_generationSession->data->pendingAccessUnit.get());
    const AVPacket* packet = scheduled
        ? FFmpegPacketView::packet(scheduled->media()) : nullptr;
    if (!scheduled || !packet || scheduled->stream() != m_outputPlan.stream ||
        !m_generationSession->data->activationFacts ||
        scheduled->generation() != m_generationSession->data->activationFacts->generation ||
        scheduled->dispatchOnMaster() < scheduled->emitOnMaster()) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "RTP protocol materializer rejects access-unit generation, stream, packet, or timing"));
    }
    auto publication = m_dependencies.authority->reserveCommit(scheduled->generation());
    if (!publication) return publication.error().code == ::media::ErrorCode::Cancelled
        ? processWaiting() : processProgress(::media::Status::failure(publication.error()));
    std::optional<MediaProtocolOutputCommitReservation> materializationPermit(std::move(publication).value());
    if (m_generationSession->data->packetizedBytes.empty()) {
        m_generationSession->data->packetizedPayloadOctets.clear();
        auto mapper = MediaRtpOutputClockMapper::create(
            m_outputPlan.clockRate, m_outputPlan.baseTimestamp,
            m_generationSession->data->activationFacts->masterRelease);
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
        auto packetized = m_generationSession->data->packetizer->writeAccessUnit(
            *packet, timestamp.value());
        if (!packetized || m_generationSession->data->packetizedBytes.empty() ||
            m_generationSession->data->packetizedBytes.size() != m_generationSession->data->packetizedPayloadOctets.size()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                packetized
                    ? invalid("RTP packetizer emitted no complete AU batch")
                    : packetized.error());
        }
        const auto initialSequence = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(m_generationSession->data->packetizedBytes.front()[2]) << 8) |
            static_cast<std::uint16_t>(m_generationSession->data->packetizedBytes.front()[3]));
        auto wireReady = createWireMaterializer(context, initialSequence);
        if (!wireReady) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                wireReady.error());
        }
    }
    std::vector<MediaPacketizedRtpDatagramView> views;
    try {
        views.reserve(m_generationSession->data->packetizedBytes.size());
        for (std::size_t index = 0; index < m_generationSession->data->packetizedBytes.size(); ++index) {
            views.push_back(MediaPacketizedRtpDatagramView{
                m_generationSession->data->packetizedBytes[index], m_generationSession->data->packetizedPayloadOctets[index],
                scheduled->presentationOnMaster(),
                scheduled->emitOnMaster(),
                scheduled->stream() == MediaScheduledStream::Video &&
                    (packet->flags & AV_PKT_FLAG_KEY) != 0 &&
                    index + 1 == m_generationSession->data->packetizedBytes.size()
                    ? MediaWireMediaBoundary::VideoRandomAccessUnitEnd
                    : MediaWireMediaBoundary::None});
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed("RTP wire AU views"));
    }
    auto materializedAt = m_dependencies.authority->now();
    if (!materializedAt) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            materializedAt.error());
    }
    auto wire = m_generationSession->data->wireMaterializer->materializeBatch(
        views, materializedAt.value());
    if (!wire) {
        return ::media::Result<MediaNodeProcessResult>::failure(wire.error());
    }
    m_generationSession->data->pendingAccessUnit.reset();
    m_generationSession->data->packetizedPayloadOctets.clear();
    m_generationSession->data->packetizedBytes.clear();
    try {
        for (auto& partition : wire.value()) {
            m_generationSession->data->pendingWireOutputs.push_back(std::move(partition));
        }
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed(
                "RTP wire output partitions"));
    }
    if (m_generationSession->data->pendingWireOutputs.empty()) {
        return ::media::Result<MediaNodeProcessResult>::failure(invalid(
            "RTP wire materializer emitted no batch partitions"));
    }
    materializationPermit.reset();
    auto emitted = emitOutput(
        context, "wire_batch", m_generationSession->data->pendingWireOutputs.front());
    return emitted ? processProgress() : processProgress(std::move(emitted));
}

::media::Result<MediaNodeProcessResult>
MediaRtpDatagramMaterializerNode::process(MediaGraphExecutionContext& context)
{
    if (auto purge = m_generationPurge->pending()) {
        auto activation = m_dependencies.authority->currentActivation();
        if (!activation || activation.value().generation != purge->oldGeneration)
            return processProgress(::media::Status::failure(activation
                ? invalid("Materializer purge differs from the globally revoked generation") : activation.error()));
        ::media::Status status = ::media::Status::success();
        if (const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
                m_generationSession->data->transportPlan.get())) {
            status = authorizeProtocolPurgeInput(m_generationSession->data->transportPlan,
                m_plannedSessionKey, *purge, m_completedPurge);
        }
        if (status) {
            static constexpr std::array<const char*, 3> ports{"activation", "transport_plan", "scheduled"};
            for (const auto* port : ports) {
                while (status) {
                    auto input = tryPopInputOptional(context, port);
                    if (!input) { status = ::media::Status::failure(input.error()); break; }
                    if (!input.value()) break;
                    status = authorizeProtocolPurgeInput(*input.value(), m_plannedSessionKey, *purge, m_completedPurge);
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
MediaRtpDatagramMaterializerNode::reserveOutputCommit(const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    const auto& facts = m_generationSession->data->activationFacts;
    std::optional<std::uint64_t> generation = facts ? std::optional(facts->generation) : std::nullopt;
    if (const auto* wire = dynamic_cast<const MediaWireDatagramBatchBuffer*>(buffer.get());
        wire && (!generation || wire->generation() != *generation))
        return Result::failure(invalid("RTP materializer wire publication differs from active generation"));
    if (!generation) return Result::failure(invalid("Protocol materializer publication lacks its active plan"));
    auto reserved = m_generationState->reserveCommit(*m_dependencies.authority, *generation);
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
MediaRtpDatagramMaterializerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_generationSession->data->pendingDescription) {
        auto emitted = emitOutput(
            context, "description", m_generationSession->data->pendingDescription);
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    if (!m_generationSession->data->pendingWireOutputs.empty()) {
        auto emitted = emitOutput(
            context, "wire_batch", m_generationSession->data->pendingWireOutputs.front());
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    if (m_generationSession->data->terminal) return finishProtocol(context);
    auto bindings = acquireBindings(context);
    if (!bindings) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            bindings.error());
    }
    if (!m_generationSession->data->activationFacts || !m_codec || !m_generationSession->data->transportPlan) {
        return bindings.value() ? processProgress() : processWaiting();
    }
    if (!m_generationSession->data->packetizer) {
        const auto* codec = dynamic_cast<const FFmpegCodecParametersBuffer*>(
            m_codec.get());
        const bool needsAccessUnit = codec && codec->parameters() &&
            (m_outputPlan.packetization.packetizationMode() ==
                 MediaScheduledRtpPacketizationMode::H264AnnexB ||
             m_outputPlan.packetization.packetizationMode() ==
                 MediaScheduledRtpPacketizationMode::HevcAnnexB) &&
            (!codec->parameters()->extradata ||
             codec->parameters()->extradata_size <= 0);
        const AVPacket* configurationPacket = nullptr;
        if (needsAccessUnit) {
            auto input = tryPopInputOptional(context, "scheduled");
            if (!input) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    input.error());
            }
            if (!input.value()) return processWaiting();
            m_generationSession->data->stagedConfigurationAccessUnit = std::move(*input.value());
            auto discarded = discardPurgedScheduled(m_generationSession->data->stagedConfigurationAccessUnit);
            if (!discarded) return processProgress(::media::Status::failure(discarded.error()));
            if (discarded.value()) return processProgress();
            if (m_generationSession->data->stagedConfigurationAccessUnit->isEof()) {
                m_generationSession->data->terminal = std::move(m_generationSession->data->stagedConfigurationAccessUnit);
                return finishProtocol(context);
            }
            const auto* scheduled =
                dynamic_cast<const MediaScheduledAccessUnit*>(
                    m_generationSession->data->stagedConfigurationAccessUnit.get());
            configurationPacket = scheduled
                ? FFmpegPacketView::packet(scheduled->media()) : nullptr;
            if (!configurationPacket) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    invalid("RTP configuration requires first access unit"));
            }
        }
        auto opened = openProtocol(configurationPacket);
        if (!opened) {
            return processProgress(::media::Status::failure(opened.error()));
        }
        auto emitted = emitOutput(
            context, "description", m_generationSession->data->pendingDescription);
        return emitted ? processProgress() : processProgress(std::move(emitted));
    }
    if (!m_generationSession->data->descriptionEmitted) return processWaiting();
    return processAccessUnit(context);
}

::media::Status MediaRtpDatagramMaterializerNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    if (m_generationSession->data->pendingDescription && buffer == m_generationSession->data->pendingDescription) {
        m_generationSession->data->descriptionEmitted = true;
        m_generationSession->data->pendingDescription.reset();
        return ::media::Status::success();
    }
    if (!m_generationSession->data->pendingWireOutputs.empty() &&
        buffer == m_generationSession->data->pendingWireOutputs.front()) {
        m_generationSession->data->pendingWireOutputs.pop_front();
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::cancelled(
        "RTP materializer output commit differs from pending output"));
}

::media::Result<MediaNodeProcessResult>
MediaRtpDatagramMaterializerNode::finishProtocol(MediaGraphExecutionContext& context)
{
    const auto generation = m_generationSession->data->activationFacts->generation;
    const auto* control = dynamic_cast<const MediaControlBuffer*>(m_generationSession->data->terminal.get());
    if (!control) return processProgress(::media::Status::failure(invalid("Protocol terminal buffer is absent")));
    auto terminal = classifyProtocolTerminalGeneration(*control, generation, m_dependencies.authority->streamSet(), m_completedPurge);
    if (!terminal) return processProgress(::media::Status::failure(terminal.error()));
    if (!terminal.value()) { m_generationSession->data->terminal.reset(); return processProgress(); }
    if (!m_generationSession->data->terminalReportPrepared) {
        auto publication = m_dependencies.authority->reserveCommit(generation);
        if (!publication) return publication.error().code == ::media::ErrorCode::Cancelled
            ? processWaiting() : processProgress(::media::Status::failure(publication.error()));
        if (auto* rtp = m_generationSession->data->wireMaterializer ? &*m_generationSession->data->wireMaterializer : nullptr) {
            auto now = m_dependencies.authority->now();
            if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
            auto report = rtp->materializeTerminalReport(now.value(), now.value(), now.value());
            if (!report) return processProgress(::media::Status::failure(report.error()));
            if (report.value()) m_generationSession->data->pendingWireOutputs.push_back(std::move(report).value());
        }
        m_generationSession->data->terminalReportPrepared = true;
    }
    if (!m_generationSession->data->pendingWireOutputs.empty())
        return processProgress(emitOutput(context, "wire_batch", m_generationSession->data->pendingWireOutputs.front()));
    auto* wire = context.findOutputChannel(nodeId(), "wire_batch");
    if (!wire) return ::media::Result<MediaNodeProcessResult>::failure(invalid(
        "protocol termination requires its planned wire output"));
    auto publication = m_dependencies.authority->reserveCommit(generation);
    if (!publication) return publication.error().code == ::media::ErrorCode::Cancelled
        ? processWaiting() : processProgress(::media::Status::failure(publication.error()));
    return processFinished(wire->closeWithTerminal(m_generationSession->data->terminal));
}

::media::Status MediaRtpDatagramMaterializerNode::stop(
    MediaGraphExecutionContext& context)
{
    m_generationPurge->stop();
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaRtpDatagramMaterializerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_generationPurge->stop();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaRtpDatagramMaterializerNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_generationSession->data->packetizedPayloadOctets.clear();
    m_generationSession->data->packetizedBytes.clear();
    m_generationSession->data->wireMaterializer.reset();
    m_generationSession->data->packetizer.reset();
    m_generationSession->data->pendingDescription.reset();
    m_generationSession->data->pendingWireOutputs.clear();
    m_generationSession->data->terminal.reset();
    m_generationSession->data->terminalReportPrepared = false;
    m_generationSession->data->descriptionEmitted = false;
    m_generationSession->data->stagedConfigurationAccessUnit.reset();
    m_generationSession->data->pendingAccessUnit.reset();
    m_generationSession->data->transportPlan.reset();
    m_codec.reset();
    m_generationSession->data->activationFacts.reset();
    m_generationSession->data->activation.reset();
}

} // namespace media::ffmpeg::graph

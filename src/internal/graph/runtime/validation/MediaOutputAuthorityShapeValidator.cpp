#include "internal/graph/runtime/validation/MediaOutputAuthorityShapeValidator.h"

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"
#include "internal/graph/runtime/validation/MediaDatagramOutputGraphShapeValidator.h"
#include "internal/graph/runtime/validation/MediaGraphShapeQuery.h"

#include <array>
#include <initializer_list>
#include <string_view>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

bool isLegacyAuthority(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RtpMux:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::VideoTimestamp:
    case MediaNodeKind::PacketStartGate:
        return true;
    default:
        return false;
    }
}

::media::Status rejectLegacyAuthorities(const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (isLegacyAuthority(node.kind)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Production synchronized output rejects legacy output and timestamp authorities"));
        }
    }
    return ::media::Status::success();
}

bool validCodecEdgeSource(
    const MediaGraph& graph,
    const MediaEdge& edge,
    MediaNodeKind sourceKind,
    MediaStreamKind stream,
    const MediaEdgePolicy& policy) noexcept
{
    const MediaNode* source = graph.findNode(edge.from.nodeId);
    const MediaPort* port = graph.findPort(edge.from.portId);
    return edge.policy == policy && source && source->kind == sourceKind &&
        port && port->nodeId == source->id && port->name == "codec" &&
        MediaGraphShapeQuery::validPort(port, MediaPortDirection::Output, stream,
                  MediaEdgeKind::Metadata,
                  MediaPayloadKind::CodecContext);
}

bool exactAudioVideoCodecEdges(
    const MediaGraph& graph,
    MediaPortId target,
    const MediaEdgePolicy& policy,
    MediaSynchronizedAudioExecutionProduct audioProduct) noexcept
{
    bool video = false;
    bool audio = false;
    std::size_t count = 0;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId != target) continue;
        ++count;
        if (!video && validCodecEdgeSource(
                graph, edge, MediaNodeKind::VideoEncode,
                MediaStreamKind::Video, policy)) {
            video = true;
        } else if (!audio && validCodecEdgeSource(
                       graph, edge,
                       audioProduct ==
                               MediaSynchronizedAudioExecutionProduct::PacketCopy
                           ? MediaNodeKind::PacketSourceConfig
                           : MediaNodeKind::AudioEncode,
                       MediaStreamKind::Audio, policy)) {
            audio = true;
        } else {
            return false;
        }
    }
    return count == 2 && video && audio;
}

::media::Status validateScheduledRtpSender(
    const MediaGraph& graph,
    const MediaNode& sender,
    const MediaAvSyncRuntimeBinding& binding,
    const MediaScheduledRtpOutputPlan& product,
    const MediaSeparateRtpSdpRuntimePlan& sdp,
    const MediaNode& publisher,
    const char* publisherPort)
{
    auto decoded = MediaScheduledRtpSenderNodePlanCodec::decode(sender);
    if (!decoded) return ::media::Status::failure(decoded.error());
    const MediaStreamKind stream = product.stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    const MediaPort* activation = sender.findInputPort("activation");
    const MediaPort* codec = sender.findInputPort("codec");
    const MediaPort* scheduled = sender.findInputPort("scheduled");
    const MediaPort* transport = sender.findInputPort("transport_plan");
    const MediaPort* description = sender.findOutputPort("description");
    const MediaPort* wire = sender.findOutputPort("wire_batch");
    const MediaPort* published = publisher.findInputPort(publisherPort);
    if (decoded.value().sessionKey !=
            MediaProtocolOutputSessionKey(binding.groupKey.value()) ||
        decoded.value().streamSet != MediaTranscodeStreamSet::AudioVideo ||
        decoded.value().output != product || decoded.value().sdp != sdp ||
        sender.inputPorts.size() != 4 || sender.outputPorts.size() != 2 ||
        !MediaGraphShapeQuery::validPort(activation, MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Event,
                   MediaPayloadKind::GraphEvent) ||
        !MediaGraphShapeQuery::validPort(codec, MediaPortDirection::Input, stream,
                   MediaEdgeKind::Metadata,
                   MediaPayloadKind::CodecContext) ||
        !MediaGraphShapeQuery::validPort(scheduled, MediaPortDirection::Input, stream,
                   MediaEdgeKind::EncodedPacket,
                   MediaPayloadKind::Packet) ||
        !MediaGraphShapeQuery::validPort(transport, MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                   MediaPayloadKind::DatagramTransportPlan) ||
        !MediaGraphShapeQuery::validPort(description, MediaPortDirection::Output,
                   MediaStreamKind::Metadata, MediaEdgeKind::Event,
                   MediaPayloadKind::GraphEvent) ||
        !MediaGraphShapeQuery::validPort(wire, MediaPortDirection::Output,
                   MediaStreamKind::Metadata,
                   MediaEdgeKind::ScheduledDatagramBatch,
                   MediaPayloadKind::WireDatagramBatch) ||
        !MediaGraphShapeQuery::validPort(published, MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Event,
                   MediaPayloadKind::GraphEvent)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender differs from its AudioVideo runtime product"));
    }
    const MediaEdge* activationEdge = MediaGraphShapeQuery::singleIncomingEdge(graph, activation->id);
    const MediaEdge* codecEdge = MediaGraphShapeQuery::singleIncomingEdge(graph, codec->id);
    const MediaEdge* scheduledEdge = MediaGraphShapeQuery::singleIncomingEdge(graph, scheduled->id);
    const MediaEdge* descriptionEdge = MediaGraphShapeQuery::singleIncomingEdge(graph, published->id);
    if (!activationEdge || !codecEdge || !scheduledEdge ||
        !descriptionEdge ||
        activationEdge->policy != binding.edgePolicies.atomicMetadata ||
        codecEdge->policy != binding.edgePolicies.metadata ||
        !validCodecEdgeSource(
            graph, *codecEdge,
            stream == MediaStreamKind::Video
                ? MediaNodeKind::VideoEncode
                : binding.audioExecutionProduct ==
                        MediaSynchronizedAudioExecutionProduct::PacketCopy
                    ? MediaNodeKind::PacketSourceConfig
                    : MediaNodeKind::AudioEncode,
            stream, binding.edgePolicies.metadata) ||
        scheduledEdge->policy !=
            (stream == MediaStreamKind::Video
                 ? binding.edgePolicies.atomicVideoPacket
                 : binding.edgePolicies.atomicAudioPacket) ||
        descriptionEdge->policy != binding.edgePolicies.metadata ||
        descriptionEdge->from.portId != description->id) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender edges differ from their AudioVideo runtime product"));
    }
    return ::media::Status::success();
}

::media::Status validateScheduledRtpOutput(
    const MediaGraph& graph,
    const MediaAvSyncGraphShape& shape,
    const MediaAvSyncRuntimeBinding& binding,
    const MediaSeparateRtpOutputRuntimePlan& product)
{
    const MediaNode& publisher =
        *shape.nodes(MediaNodeKind::RtpSdpPublisher).front();
    if (!MediaGraphShapeQuery::hasExactOptionKeys(publisher.options, {"sdp.path", "sdp.stream_set"}) ||
        publisher.options.value("sdp.path") != product.sdp.path ||
        !MediaGraphShapeQuery::matchesStreamSetOption(
            publisher.options, "sdp.stream_set",
            MediaTranscodeStreamSet::AudioVideo) ||
        publisher.inputPorts.size() != 2 ||
        !publisher.outputPorts.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioVideo RTP SDP publisher differs from its runtime product"));
    }
    const MediaNode* video = nullptr;
    const MediaNode* audio = nullptr;
    for (const MediaNode* sender :
         shape.nodes(MediaNodeKind::RtpDatagramMaterializer)) {
        auto decoded = MediaScheduledRtpSenderNodePlanCodec::decode(*sender);
        if (!decoded) return ::media::Status::failure(decoded.error());
        if (decoded.value().output.stream == MediaScheduledStream::Video) {
            if (video) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "AudioVideo RTP output rejects duplicate video senders"));
            }
            video = sender;
        } else {
            if (audio) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "AudioVideo RTP output rejects duplicate audio senders"));
            }
            audio = sender;
        }
    }
    if (!video || !audio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioVideo RTP output requires one video and one audio sender"));
    }
    if (auto valid = validateScheduledRtpSender(
            graph, *video, binding, product.video, product.sdp,
            publisher, "video"); !valid) {
        return valid;
    }
    return validateScheduledRtpSender(
        graph, *audio, binding, product.audio, product.sdp,
        publisher, "audio");
}

::media::Status validateProjectMux(
    const MediaNode& mux,
    const MediaProjectMpegTsRuntimeOutputPlan& product,
    bool requireByteSink)
{
    if (product.muxSessionKind !=
            MediaMuxSessionKind::ProjectMpegTs ||
        !MediaGraphShapeQuery::hasExactOptionKeys(
            mux.options,
            {MediaTranscodeOptionKey::MuxExpectVideo,
             MediaTranscodeOptionKey::MuxExpectAudio,
             MediaTranscodeOptionKey::MuxSessionKind})) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux node options conflict with its planner product"));
    }
    auto expectVideo = requiredBoolNodeOption(
        &mux.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::MuxExpectVideo);
    auto expectAudio = requiredBoolNodeOption(
        &mux.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::MuxExpectAudio);
    auto sessionText = requiredNodeOption(
        &mux.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::MuxSessionKind);
    if (!expectVideo || !expectAudio || !sessionText) {
        return !expectVideo
            ? ::media::Status::failure(expectVideo.error())
            : !expectAudio
                ? ::media::Status::failure(expectAudio.error())
                : ::media::Status::failure(sessionText.error());
    }
    auto session =
        parseMediaMuxSessionKindOption(sessionText.value());
    if (!session || !expectVideo.value() || !expectAudio.value() ||
        session.value() != product.muxSessionKind) {
        return ::media::Status::failure(
            session
                ? ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS mux session and stream set conflict with planner facts")
                : session.error());
    }
    const std::size_t expectedInputs = 3;
    const std::size_t expectedOutputs = 1;
    if (mux.inputPorts.size() != expectedInputs ||
        mux.outputPorts.size() != expectedOutputs ||
        !MediaGraphShapeQuery::validPort(
            mux.findInputPort("codec"), MediaPortDirection::Input,
            MediaStreamKind::Any, MediaEdgeKind::Metadata,
            MediaPayloadKind::CodecContext) ||
        !MediaGraphShapeQuery::validPort(
            mux.findInputPort("packet"), MediaPortDirection::Input,
            MediaStreamKind::Any, MediaEdgeKind::EncodedPacket,
            MediaPayloadKind::TsAccessUnit) ||
        !MediaGraphShapeQuery::validPort(
            mux.findInputPort("plan"), MediaPortDirection::Input,
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        mux.findInputPort("resource") ||
        !MediaGraphShapeQuery::validPort(
             mux.findOutputPort("batch"), MediaPortDirection::Output,
             MediaStreamKind::Metadata,
             MediaEdgeKind::ScheduledDatagramBatch,
             MediaPayloadKind::MpegTsProtocolDatagramBatch)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux ports conflict with its transport product"));
    }
    return ::media::Status::success();
}

::media::Status validateAudioVideoProjectNodes(
    const MediaGraph& graph,
    const MediaAvSyncGraphShape& shape,
    const MediaAvSyncRuntimeBinding& binding,
    bool requireByteSink)
{
    const MediaNode& source =
        *shape.nodes(MediaNodeKind::ProjectMpegTsPlanSource).front();
    const MediaNode& adapter =
        *shape.nodes(MediaNodeKind::ScheduledTsAccessUnitAdapter).front();
    const MediaNode& mux = *shape.nodes(MediaNodeKind::FileMux).front();
    const MediaNode& materializer =
        *shape.nodes(MediaNodeKind::MpegTsDatagramMaterializer).front();
    const MediaPort* sourceActivation = source.findInputPort("activation");
    const MediaPort* sourcePlan = source.findOutputPort("plan");
    const MediaPort* adapterPlan = adapter.findInputPort("plan");
    const MediaPort* adapterScheduled = adapter.findInputPort("scheduled");
    const MediaPort* adapterPacket = adapter.findOutputPort("packet");
    const MediaPort* muxCodec = mux.findInputPort("codec");
    const MediaPort* muxPacket = mux.findInputPort("packet");
    const MediaPort* muxPlan = mux.findInputPort("plan");
    const MediaPort* materializerPlan =
        materializer.findInputPort("protocol_plan");
    const MediaPort* materializerBatch =
        materializer.findInputPort("protocol_batch");
    const MediaPort* muxBatch = mux.findOutputPort("batch");
    if (source.inputPorts.size() != 1 || source.outputPorts.size() != 1 ||
        !MediaGraphShapeQuery::validPort(sourceActivation, MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Event,
                   MediaPayloadKind::GraphEvent) ||
        !MediaGraphShapeQuery::validPort(sourcePlan, MediaPortDirection::Output,
                   MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        adapter.inputPorts.size() != 2 || adapter.outputPorts.size() != 1 ||
        !MediaGraphShapeQuery::hasExactOptionKeys(adapter.options,
                   {"scheduled_ts_adapter.session",
                    "scheduled_ts_adapter.stream_set"}) ||
        adapter.options.value("scheduled_ts_adapter.session") !=
            binding.groupKey.value() ||
        !MediaGraphShapeQuery::matchesStreamSetOption(
            adapter.options, "scheduled_ts_adapter.stream_set",
            MediaTranscodeStreamSet::AudioVideo) ||
        !MediaGraphShapeQuery::validPort(adapterPlan, MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        !MediaGraphShapeQuery::validPort(adapterScheduled, MediaPortDirection::Input,
                   MediaStreamKind::Any, MediaEdgeKind::EncodedPacket,
                   MediaPayloadKind::Packet) ||
        !MediaGraphShapeQuery::validPort(adapterPacket, MediaPortDirection::Output,
                   MediaStreamKind::Any, MediaEdgeKind::EncodedPacket,
                   MediaPayloadKind::TsAccessUnit) ||
        !muxCodec || !muxPacket || !muxPlan || !materializerPlan ||
        !materializerBatch || !muxBatch ||
        materializer.inputPorts.size() != 3 ||
        materializer.outputPorts.size() != 1 ||
        !MediaGraphShapeQuery::validPort(
            materializerPlan, MediaPortDirection::Input,
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        !MediaGraphShapeQuery::validPort(
            materializerBatch, MediaPortDirection::Input,
            MediaStreamKind::Metadata,
            MediaEdgeKind::ScheduledDatagramBatch,
            MediaPayloadKind::MpegTsProtocolDatagramBatch) ||
        !exactAudioVideoCodecEdges(
            graph, muxCodec->id, binding.edgePolicies.metadata,
            binding.audioExecutionProduct)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioVideo Project MPEG-TS nodes differ from their runtime product"));
    }
    const MediaEdge* activation = MediaGraphShapeQuery::singleIncomingEdge(
        graph, sourceActivation->id);
    const MediaEdge* scheduled = MediaGraphShapeQuery::singleIncomingEdge(
        graph, adapterScheduled->id);
    const MediaEdge* planToAdapter = MediaGraphShapeQuery::singleIncomingEdge(
        graph, adapterPlan->id);
    const MediaEdge* planToMux = MediaGraphShapeQuery::singleIncomingEdge(graph, muxPlan->id);
    const MediaEdge* packetToMux = MediaGraphShapeQuery::singleIncomingEdge(graph, muxPacket->id);
    const MediaEdge* planToMaterializer =
        MediaGraphShapeQuery::singleIncomingEdge(graph, materializerPlan->id);
    const MediaEdge* batchToMaterializer =
        MediaGraphShapeQuery::singleIncomingEdge(graph, materializerBatch->id);
    if (!activation || !scheduled || !planToAdapter || !planToMux ||
        !packetToMux || !planToMaterializer || !batchToMaterializer ||
        activation->policy != binding.edgePolicies.atomicMetadata ||
        scheduled->policy != binding.edgePolicies.synchronizedPacket ||
        planToAdapter->policy != binding.edgePolicies.atomicMetadata ||
        planToMux->policy != binding.edgePolicies.atomicMetadata ||
        packetToMux->policy != binding.edgePolicies.synchronizedPacket ||
        planToMaterializer->policy != binding.edgePolicies.atomicMetadata ||
        batchToMaterializer->policy !=
            binding.edgePolicies.synchronizedPacket ||
        planToAdapter->from.portId != sourcePlan->id ||
        planToMux->from.portId != sourcePlan->id ||
        packetToMux->from.portId != adapterPacket->id ||
        planToMaterializer->from.portId != sourcePlan->id ||
        batchToMaterializer->from.portId != muxBatch->id) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioVideo Project MPEG-TS edges differ from their runtime product"));
    }
    if (requireByteSink != (mux.findInputPort("resource") != nullptr)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioVideo Project MPEG-TS resource port conflicts with its transport"));
    }
    return ::media::Status::success();
}

::media::Status validatePlannerProduct(
    const MediaAvSyncRuntimeBinding& binding,
    const MediaProjectMpegTsRuntimeOutputPlan& product)
{
    const auto* program = product.protocol.muxPlan().audioVideoProgram();
    const bool sampleRateMatches = program &&
        program->aac.samplingFrequencyIndex < MediaAacSampleRates.size() &&
        binding.plan.audioServo.outputSampleRate &&
        *binding.plan.audioServo.outputSampleRate ==
            MediaAacSampleRates[program->aac.samplingFrequencyIndex];
    if (!binding.plan.projectMpegTsOutput ||
        !binding.plan.projectMpegTsOutput->outputMux ||
        binding.plan.rtpOutput ||
        product.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
        binding.plan.projectMpegTsOutput->outputMux->parameters() !=
            product.protocol.muxPlan().parameters() ||
        !sampleRateMatches) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS runtime output product conflicts with its synchronization planner product"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaOutputAuthorityShapeValidator::validate(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    const MediaAvSyncGraphShape shape(graph);
    if (std::holds_alternative<MediaAvSyncComponentCoreRuntimeProduct>(
            binding.outputProduct)) {
        return shape.requireExact({
            {MediaNodeKind::ScheduledRtpSender, 0,
             "scheduled RTP sender"},
            {MediaNodeKind::RtpSdpPublisher, 0,
             "RTP SDP publisher"},
            {MediaNodeKind::ScheduledTsAccessUnitAdapter, 0,
             "scheduled TS adapter"},
            {MediaNodeKind::ProjectMpegTsPlanSource, 0,
             "Project MPEG-TS plan source"},
            {MediaNodeKind::MpegTsRtpSdpPublisher, 0,
             "MP2T SDP publisher"},
            {MediaNodeKind::ScheduledDatagramSender, 0,
             "scheduled datagram sender"},
            {MediaNodeKind::DatagramTransportPlanSource, 0,
             "datagram transport plan source"},
            {MediaNodeKind::DatagramShaper, 0, "datagram shaper"},
            {MediaNodeKind::RtpDatagramMaterializer, 0,
             "RTP datagram materializer"},
            {MediaNodeKind::MpegTsDatagramMaterializer, 0,
             "MPEG-TS datagram materializer"},
            {MediaNodeKind::FileMux, 0, "output mux"},
            {MediaNodeKind::FileOutput, 0, "output resource"}},
            "component A/V core output shape");
    }
    if (auto legacy = rejectLegacyAuthorities(graph); !legacy) {
        return legacy;
    }
    if (const auto* separateRtpOutputProduct =
            std::get_if<MediaSeparateRtpOutputRuntimePlan>(
                &binding.outputProduct)) {
        if (!binding.plan.rtpOutput ||
            binding.plan.projectMpegTsOutput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Separate RTP output conflicts with planner authority facts"));
        }
        auto cardinality = shape.requireExact({
            {MediaNodeKind::ScheduledRtpSender, 0,
             "legacy scheduled RTP sender"},
            {MediaNodeKind::RtpDatagramMaterializer, 2,
             "RTP datagram materializer"},
            {MediaNodeKind::RtpSdpPublisher, 1,
             "RTP SDP publisher"},
            {MediaNodeKind::ScheduledTsAccessUnitAdapter, 0,
             "scheduled TS adapter"},
            {MediaNodeKind::ProjectMpegTsPlanSource, 0,
             "Project MPEG-TS plan source"},
            {MediaNodeKind::MpegTsRtpSdpPublisher, 0,
             "MP2T SDP publisher"},
            {MediaNodeKind::ScheduledDatagramSender, 1,
             "common scheduled datagram sender"},
            {MediaNodeKind::DatagramTransportPlanSource, 1,
             "datagram transport plan source"},
            {MediaNodeKind::DatagramShaper, 1, "shared datagram shaper"},
            {MediaNodeKind::MpegTsDatagramMaterializer, 0,
             "MPEG-TS datagram materializer"},
            {MediaNodeKind::FileMux, 0, "Project MPEG-TS mux"},
            {MediaNodeKind::FileOutput, 0, "UDP output resource"}},
            "separate RTP output shape");
        if (!cardinality) return cardinality;
        if (auto datagram = MediaDatagramOutputGraphShapeValidator::validate(
                graph, binding.datagramTransport,
                MediaProtocolOutputSessionKey(binding.groupKey.value()),
                MediaTranscodeStreamSet::AudioVideo,
                MediaNodeKind::RtpDatagramMaterializer, 2,
                binding.edgePolicies); !datagram) {
            return datagram;
        }
        return validateScheduledRtpOutput(
            graph, shape, binding,
            *separateRtpOutputProduct);
    }
    const auto* projectMpegTsOutputProduct =
        std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(
            &binding.outputProduct);
    if (!projectMpegTsOutputProduct) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS output requires its complete runtime product"));
    }
    const auto& product =
        *projectMpegTsOutputProduct;
    if (auto exact = validatePlannerProduct(binding, product);
        !exact) {
        return exact;
    }
    auto common = shape.requireExact({
        {MediaNodeKind::ScheduledRtpSender, 0,
         "scheduled RTP sender"},
        {MediaNodeKind::RtpDatagramMaterializer, 0,
         "RTP datagram materializer"},
        {MediaNodeKind::RtpSdpPublisher, 0,
         "RTP SDP publisher"},
        {MediaNodeKind::ScheduledTsAccessUnitAdapter, 1,
         "scheduled TS adapter"},
        {MediaNodeKind::ProjectMpegTsPlanSource, 1,
         "Project MPEG-TS plan source"},
        {MediaNodeKind::MpegTsDatagramMaterializer, 1,
         "MPEG-TS datagram materializer"},
        {MediaNodeKind::DatagramTransportPlanSource, 1,
         "datagram transport plan source"},
        {MediaNodeKind::DatagramShaper, 1, "shared datagram shaper"},
        {MediaNodeKind::ScheduledDatagramSender, 1,
         "common scheduled datagram sender"},
        {MediaNodeKind::FileOutput, 0, "legacy UDP byte sink"},
        {MediaNodeKind::FileMux, 1, "Project MPEG-TS mux"}},
        "Project MPEG-TS output shape");
    if (!common) return common;
    if (auto datagram = MediaDatagramOutputGraphShapeValidator::validate(
            graph, binding.datagramTransport,
            MediaProtocolOutputSessionKey(binding.groupKey.value()),
            MediaTranscodeStreamSet::AudioVideo,
            MediaNodeKind::MpegTsDatagramMaterializer, 1,
            binding.edgePolicies); !datagram) {
        return datagram;
    }
    const MediaNode& planSource =
        *shape.nodes(
            MediaNodeKind::ProjectMpegTsPlanSource).front();
    auto decoded =
        MediaProjectMpegTsPlanSourceNodePlanCodec::decode(
            planSource);
    if (!decoded) {
        return ::media::Status::failure(decoded.error());
    }
    if (auto exact =
            MediaProjectMpegTsPlanSourceNodePlanCodec::
                validateAgainstPlanner(
                    decoded.value(),
                    MediaProtocolOutputSessionKey(
                        binding.groupKey.value()),
                    MediaTranscodeStreamSet::AudioVideo,
                    product);
        !exact) {
        return exact;
    }
    const MediaNode& mux =
        *shape.nodes(MediaNodeKind::FileMux).front();
    if (std::holds_alternative<MediaMpegTsUdpOutputPlan>(
            product.transport)) {
        auto cardinality = shape.requireExact({
            {MediaNodeKind::FileOutput, 0, "legacy UDP output resource"},
            {MediaNodeKind::ScheduledDatagramSender, 1,
             "common scheduled datagram sender"},
            {MediaNodeKind::MpegTsRtpSdpPublisher, 0,
             "MP2T SDP publisher"}},
            "Project MPEG-TS UDP output shape");
        if (!cardinality) return cardinality;
        if (product.protocol.muxPlan().parameters().transportKind !=
                MediaOutputTransportKind::UdpDatagrams) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS UDP variant conflicts with mux transport"));
        }
        if (auto validMux =
                validateProjectMux(mux, product, false);
            !validMux) {
            return validMux;
        }
        if (auto nodes = validateAudioVideoProjectNodes(
                graph, shape, binding, false); !nodes) {
            return nodes;
        }
        return ::media::Status::success();
    }
    const auto* rtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(
            &product.transport);
    if (!rtp ||
        product.protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::RtpAvp) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP variant conflicts with mux transport"));
    }
    auto cardinality = shape.requireExact({
        {MediaNodeKind::FileOutput, 0, "UDP output resource"},
        {MediaNodeKind::ScheduledDatagramSender, 1,
         "scheduled datagram sender"},
        {MediaNodeKind::MpegTsRtpSdpPublisher, 1,
         "MP2T SDP publisher"}},
        "Project MPEG-TS RTP output shape");
    if (!cardinality) return cardinality;
    if (auto validMux = validateProjectMux(mux, product, false);
        !validMux) {
        return validMux;
    }
    if (auto nodes = validateAudioVideoProjectNodes(
            graph, shape, binding, false); !nodes) {
        return nodes;
    }
    const MediaNode& publisher =
        *shape.nodes(MediaNodeKind::MpegTsRtpSdpPublisher).front();
    const MediaNode& source =
        *shape.nodes(MediaNodeKind::ProjectMpegTsPlanSource).front();
    const MediaPort* publisherPlan = publisher.findInputPort("plan");
    const MediaPort* sourcePlan = source.findOutputPort("plan");
    const MediaEdge* planEdge = publisherPlan
        ? MediaGraphShapeQuery::singleIncomingEdge(graph, publisherPlan->id)
        : nullptr;
    if (!MediaGraphShapeQuery::hasExactOptionKeys(publisher.options,
                   {"mpegts_rtp_sdp.session",
                    "mpegts_rtp_sdp.stream_set"}) ||
        publisher.options.value("mpegts_rtp_sdp.session") !=
            binding.groupKey.value() ||
        !MediaGraphShapeQuery::matchesStreamSetOption(
            publisher.options, "mpegts_rtp_sdp.stream_set",
            MediaTranscodeStreamSet::AudioVideo) ||
        publisher.inputPorts.size() != 1 ||
        !publisher.outputPorts.empty() ||
        !MediaGraphShapeQuery::validPort(publisherPlan, MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        !sourcePlan || !planEdge ||
        planEdge->from.portId != sourcePlan->id ||
        planEdge->policy != binding.edgePolicies.atomicMetadata) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioVideo MP2T SDP publisher differs from its runtime product"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

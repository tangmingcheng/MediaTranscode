#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaDatagramOutputExecutionSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaProjectMpegTsMuxSegmentBuilder.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner = "MediaScheduledMpegTsOutputSegmentBuilder";

struct CommonOptions final {
    std::string prefix;
    MediaEndpoint activation;
    MediaEndpoint videoCodec;
    std::optional<MediaEndpoint> audioCodec;
    MediaEndpoint scheduled;
};

struct CommonPlan final {
    MediaProtocolOutputSessionKey sessionKey;
    MediaTranscodeStreamSet streamSet;
    const MediaProjectMpegTsRuntimeOutputPlan& output;
    const MediaGraphQueueParameters& queues;
    const MediaRealtimeEdgePolicySet& edgePolicies;
    const MediaDatagramTransportPlanTemplate& datagramTransport;
};

::media::Status requireSource(
    const MediaGraph& graph,
    const MediaEndpoint& endpoint,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload)
{
    const MediaPort* port = endpoint.valid()
        ? graph.findOutputPort(endpoint.node, endpoint.port)
        : nullptr;
    if (!port || port->streamKind != stream || port->edgeKind != edge ||
        port->payloadKind != payload) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output received an endpoint with the wrong type"));
    }
    return ::media::Status::success();
}

::media::Status setStreamSetOption(
    MediaGraph& graph,
    MediaNodeId node,
    const char* sessionOption,
    const char* streamSetOption,
    const CommonPlan& plan)
{
    auto session = MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, node, sessionOption, plan.sessionKey.value());
    if (!session) return ::media::Status::failure(session.error());
    auto streamSet = MediaTranscodeStreamSetCodec::encode(plan.streamSet);
    if (!streamSet) return ::media::Status::failure(streamSet.error());
    return MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, node, streamSetOption, std::string(streamSet.value()));
}

::media::Result<MediaScheduledMpegTsOutputSegmentResult> buildCommon(
    MediaGraph& graph,
    const CommonOptions& options,
    const CommonPlan& plan)
{
    using Result = ::media::Result<MediaScheduledMpegTsOutputSegmentResult>;
    auto streamSet = MediaTranscodeStreamSetCodec::encode(plan.streamSet);
    if (!streamSet) return Result::failure(streamSet.error());
    const bool expectAudio =
        plan.streamSet == MediaTranscodeStreamSet::AudioVideo;
    const bool typedVideoOnly =
        plan.output.protocol.muxPlan().videoOnlyProgram() != nullptr;
    if (options.prefix.empty() || !plan.sessionKey.valid() ||
        expectAudio != options.audioCodec.has_value() ||
        typedVideoOnly == expectAudio) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output requires one exact stream-set product"));
    }
    for (const auto& fact : {
             std::tuple{options.activation, MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent},
             std::tuple{options.videoCodec, MediaStreamKind::Video,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext},
             std::tuple{options.scheduled,
                        expectAudio ? MediaStreamKind::Any
                                    : MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket,
                        MediaPayloadKind::Packet}}) {
        auto valid = requireSource(
            graph, std::get<0>(fact), std::get<1>(fact),
            std::get<2>(fact), std::get<3>(fact));
        if (!valid) return Result::failure(valid.error());
    }
    if (options.audioCodec) {
        auto valid = requireSource(
            graph, *options.audioCodec, MediaStreamKind::Audio,
            MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
        if (!valid) return Result::failure(valid.error());
    }
    const bool duplicate = std::any_of(
        graph.nodes().begin(), graph.nodes().end(), [](const MediaNode& node) {
            return node.kind == MediaNodeKind::ProjectMpegTsPlanSource ||
                   node.kind == MediaNodeKind::ScheduledTsAccessUnitAdapter ||
                   node.kind == MediaNodeKind::ScheduledDatagramSender ||
                   node.kind == MediaNodeKind::DatagramTransportPlanSource ||
                   node.kind == MediaNodeKind::MpegTsDatagramMaterializer ||
                   node.kind == MediaNodeKind::MpegTsRtpSdpPublisher;
        });
    if (duplicate) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output rejects duplicate output authority"));
    }

    MediaNodeId udpOutput = MediaNodeId::invalid();
    MediaNodeId mux = MediaNodeId::invalid();
    MediaNodeId scheduledDatagramSender = MediaNodeId::invalid();
    MediaNodeId rtpSdpPublisher = MediaNodeId::invalid();
    const bool udpTransport = std::holds_alternative<MediaMpegTsUdpOutputPlan>(
        plan.output.transport);
    const bool rtpTransport = std::holds_alternative<MediaMpegTsRtpOutputPlan>(
        plan.output.transport);
    const MediaOutputTransportKind expectedTransport = udpTransport
        ? MediaOutputTransportKind::UdpDatagrams
        : MediaOutputTransportKind::RtpAvp;
    if ((!udpTransport && !rtpTransport) ||
        plan.output.protocol.muxPlan().parameters().transportKind !=
            expectedTransport) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output requires one exact datagram transport"));
    }
    auto addedMux = MediaProjectMpegTsMuxSegmentBuilder::build(
        graph, MediaProjectMpegTsMuxSegmentOptions{
            options.prefix, true, expectAudio,
            plan.output.muxSessionKind, false, true});
    if (!addedMux) return Result::failure(addedMux.error());
    mux = addedMux.value();
    if (rtpTransport) {
        rtpSdpPublisher = graph.addNode(
            MediaNodeKind::MpegTsRtpSdpPublisher,
            options.prefix + ".rtp.sdp.publisher",
            "Atomic MP2T RTP SDP publisher");
        if (!rtpSdpPublisher.isValid()) {
            return Result::failure(::media::ErrorInfo::internalError(
                "Scheduled MPEG-TS RTP output failed to add its SDP publisher"));
        }
        auto identity = setStreamSetOption(
            graph, rtpSdpPublisher, "mpegts_rtp_sdp.session",
            "mpegts_rtp_sdp.stream_set", plan);
        if (!identity) return Result::failure(identity.error());
        auto planPort = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, rtpSdpPublisher, "plan",
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan, true, false);
        if (!planPort) return Result::failure(planPort.error());
    }

    const MediaNodeId planSource = graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource,
        options.prefix + ".plan.source", "Activated project MPEG-TS plan");
    const MediaNodeId adapter = graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter,
        options.prefix + ".scheduled.adapter", "Scheduled MPEG-TS AU adapter");
    const MediaNodeId materializer = graph.addNode(
        MediaNodeKind::MpegTsDatagramMaterializer,
        options.prefix + ".datagram.materializer",
        "MPEG-TS wire protocol materializer");
    if (!planSource.isValid() || !adapter.isValid() ||
        !materializer.isValid()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "Scheduled MPEG-TS output failed to add its nodes"));
    }
    auto encoded = MediaProjectMpegTsPlanSourceNodePlanCodec::apply(
        graph, planSource, plan.sessionKey, plan.streamSet, plan.output);
    if (!encoded) return Result::failure(encoded.error());
    auto adapterIdentity = setStreamSetOption(
        graph, adapter, "scheduled_ts_adapter.session",
        "scheduled_ts_adapter.stream_set", plan);
    if (!adapterIdentity) return Result::failure(adapterIdentity.error());
    auto execution = MediaDatagramOutputExecutionSegmentBuilder::build(
        graph, {options.prefix, options.activation, plan.sessionKey,
                plan.streamSet, &plan.datagramTransport,
                &plan.edgePolicies});
    if (!execution) return Result::failure(execution.error());
    scheduledDatagramSender = execution.value().sender;

    using MediaGraphBuildSupport::addInputPortChecked;
    using MediaGraphBuildSupport::addOutputPortChecked;
    if (auto status = addInputPortChecked(
            graph, Owner, planSource, "activation", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
        !status) return Result::failure(status.error());
    if (auto status = addOutputPortChecked(
            graph, Owner, planSource, "plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan, true, true);
        !status) return Result::failure(status.error());
    if (auto status = addInputPortChecked(
            graph, Owner, adapter, "plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan, true, false);
        !status) return Result::failure(status.error());
    if (auto status = addInputPortChecked(
            graph, Owner, adapter, "scheduled",
            expectAudio ? MediaStreamKind::Any : MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet,
            true, false); !status) return Result::failure(status.error());
    if (auto status = addOutputPortChecked(
            graph, Owner, adapter, "packet", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::TsAccessUnit,
            true, false); !status) return Result::failure(status.error());
    for (const auto& port : {
             std::tuple{"protocol_plan", MediaEdgeKind::Metadata,
                        MediaPayloadKind::ProjectMpegTsRuntimePlan},
             std::tuple{"transport_plan", MediaEdgeKind::Metadata,
                        MediaPayloadKind::DatagramTransportPlan},
             std::tuple{"protocol_batch", MediaEdgeKind::ScheduledDatagramBatch,
                        MediaPayloadKind::MpegTsProtocolDatagramBatch}}) {
        auto status = addInputPortChecked(
            graph, Owner, materializer, std::get<0>(port),
            MediaStreamKind::Metadata, std::get<1>(port), std::get<2>(port),
            true, false);
        if (!status) return Result::failure(status.error());
    }
    if (auto status = addOutputPortChecked(
            graph, Owner, materializer, "wire_batch",
            MediaStreamKind::Metadata, MediaEdgeKind::ScheduledDatagramBatch,
            MediaPayloadKind::WireDatagramBatch, true, false); !status) {
        return Result::failure(status.error());
    }

    const auto connect = [&](const MediaEndpoint& from, MediaNodeId to,
                             const char* port, const char* label,
                             const MediaEdgePolicy& policy) {
        return MediaGraphBuildSupport::connectChecked(
            graph, Owner, from.node, from.port, to, port, label, policy);
    };
    for (const auto& connection : {
             std::tuple{options.activation, planSource, "activation",
                        "output activation -> MPEG-TS plan",
                        plan.edgePolicies.atomicMetadata},
             std::tuple{options.videoCodec, mux, "codec",
                        "video codec -> MPEG-TS mux",
                        plan.edgePolicies.metadata},
             std::tuple{options.scheduled, adapter, "scheduled",
                        "scheduled media -> MPEG-TS adapter",
                        plan.edgePolicies.synchronizedPacket}}) {
        auto connected = connect(
            std::get<0>(connection), std::get<1>(connection),
            std::get<2>(connection), std::get<3>(connection),
            std::get<4>(connection));
        if (!connected) return Result::failure(connected.error());
    }
    if (options.audioCodec) {
        auto connected = connect(
            *options.audioCodec, mux, "codec",
            "audio codec -> MPEG-TS mux", plan.edgePolicies.metadata);
        if (!connected) return Result::failure(connected.error());
    }
    for (const auto& target : {
             std::pair{mux, "MPEG-TS plan -> mux"},
             std::pair{adapter, "MPEG-TS plan -> adapter"},
             std::pair{materializer, "MPEG-TS plan -> materializer"}}) {
        auto connected = MediaGraphBuildSupport::connectChecked(
            graph, Owner, planSource, "plan", target.first,
            target.first == materializer ? "protocol_plan" : "plan",
            target.second, plan.edgePolicies.atomicMetadata);
        if (!connected) return Result::failure(connected.error());
    }
    auto packet = MediaGraphBuildSupport::connectChecked(
        graph, Owner, adapter, "packet", mux, "packet",
        "scheduled TS AU -> project mux",
        plan.edgePolicies.synchronizedPacket);
    if (!packet) return Result::failure(packet.error());
    auto transport =
        MediaDatagramOutputExecutionSegmentBuilder::connectTransportConsumer(
            graph, execution.value(), materializer, "transport_plan",
            plan.edgePolicies.atomicMetadata,
            "transport plan -> MPEG-TS materializer");
    if (!transport) return Result::failure(transport.error());
    auto protocolBatch = MediaGraphBuildSupport::connectChecked(
        graph, Owner, mux, "batch", materializer, "protocol_batch",
        "MPEG-TS protocol datagrams -> wire materializer",
        plan.edgePolicies.synchronizedPacket);
    if (!protocolBatch) return Result::failure(protocolBatch.error());
    auto wire = MediaDatagramOutputExecutionSegmentBuilder::connectWireSource(
        graph, execution.value(), {materializer, "wire_batch"},
        plan.edgePolicies.synchronizedPacket,
        "MPEG-TS wire datagrams -> shared shaper");
    if (!wire) return Result::failure(wire.error());
    if (rtpSdpPublisher.isValid()) {
        auto connected = MediaGraphBuildSupport::connectChecked(
            graph, Owner, planSource, "plan", rtpSdpPublisher, "plan",
            "MPEG-TS RTP plan -> SDP publisher",
            plan.edgePolicies.atomicMetadata);
        if (!connected) return Result::failure(connected.error());
    }
    return Result::success(
        {planSource, adapter, udpOutput, mux,
         scheduledDatagramSender, rtpSdpPublisher});
}

} // namespace

::media::Result<MediaScheduledMpegTsOutputSegmentResult>
MediaScheduledMpegTsOutputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaScheduledMpegTsOutputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (!options.expectVideo || !options.expectAudio ||
        !plan.groupKey.valid() ||
        plan.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs ||
        !std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(
            plan.protocolOutput)) {
        return ::media::Result<MediaScheduledMpegTsOutputSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V MPEG-TS output requires its complete runtime product"));
    }
    return buildCommon(
        graph,
        CommonOptions{options.prefix, options.epochActivated,
                      options.videoCodec, options.audioCodec,
                      options.scheduled},
        CommonPlan{MediaProtocolOutputSessionKey(plan.groupKey.value()),
                   MediaTranscodeStreamSet::AudioVideo,
                   std::get<MediaProjectMpegTsRuntimeOutputPlan>(
                       plan.protocolOutput),
                   plan.queues, plan.edgePolicies,
                   plan.datagramTransport});
}

::media::Result<MediaScheduledMpegTsOutputSegmentResult>
MediaScheduledMpegTsOutputSegmentBuilder::buildVideoOnly(
    MediaGraph& graph,
    const MediaVideoOnlyScheduledMpegTsOutputSegmentOptions& options,
    const MediaRealtimeVideoRuntimePlan& plan)
{
    const auto* output = std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(
        &plan.outputAdapter);
    if (!output || !plan.sessionKey.valid()) {
        return ::media::Result<MediaScheduledMpegTsOutputSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly MPEG-TS output requires its complete runtime product"));
    }
    return buildCommon(
        graph,
        CommonOptions{options.prefix, options.activation,
                      options.videoCodec, std::nullopt,
                      options.scheduledVideo},
        CommonPlan{plan.sessionKey, MediaTranscodeStreamSet::VideoOnly,
                   *output, plan.queues, plan.edgePolicies,
                   plan.datagramTransport});
}

} // namespace media::ffmpeg::graph

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
#include <utility>

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
                        MediaEdgeKind::Metadata, expectAudio ? MediaPayloadKind::CodecContext : MediaPayloadKind::CodecParameters},
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
    if (expectAudio && duplicate) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output rejects duplicate output authority"));
    }

    if (auto available = MediaDatagramOutputExecutionSegmentBuilder::validateSessionAvailable(
            graph, plan.sessionKey); !available) {
        return Result::failure(available.error());
    }

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
    auto declaration=MediaOutputSegmentPlanner::mpegTs(expectAudio,rtpTransport,
        expectAudio?MediaPayloadKind::CodecContext:MediaPayloadKind::CodecParameters,plan.edgePolicies, *std::pmr::new_delete_resource());
    if(!declaration)return Result::failure(declaration.error());
    auto addedMux = MediaProjectMpegTsMuxSegmentBuilder::build(
        graph, MediaProjectMpegTsMuxSegmentOptions{
            options.prefix, true, expectAudio,
            plan.output.muxSessionKind, false, true});
    if (!addedMux) return Result::failure(addedMux.error());
    mux = addedMux.value();
    if (!expectAudio) graph.findInputPort(mux, "codec")->payloadKind = MediaPayloadKind::CodecParameters;
    if (rtpTransport) {
        rtpSdpPublisher = graph.addNode(
            *declaration.value().kind(MediaOutputSegmentRole::Sdp),
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
        *declaration.value().kind(MediaOutputSegmentRole::TsPlan),
        options.prefix + ".plan.source", "Activated project MPEG-TS plan");
    const MediaNodeId adapter = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::TsAdapter),
        options.prefix + ".scheduled.adapter", "Scheduled MPEG-TS AU adapter");
    const MediaNodeId materializer = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::TsMaterializer),
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

    auto transportInput = addInputPortChecked(
        graph, Owner, planSource, "transport_plan", MediaStreamKind::Metadata,
        MediaEdgeKind::Metadata, MediaPayloadKind::DatagramTransportPlan, true, false);
    if (!transportInput) return Result::failure(transportInput.error());
    MediaOutputSegmentBindings bindings{std::move(declaration).value(),{}};
    using Role=MediaOutputSegmentRole;
    bindings.endpoint(Role::Activation)=options.activation;
    bindings.endpoint(Role::VideoCodec)=options.videoCodec;
    if(options.audioCodec)bindings.endpoint(Role::AudioCodec)=*options.audioCodec;
    bindings.endpoint(Role::SerializedPacket)=options.scheduled;
    bindings.endpoint(Role::TsPlan)={planSource,""};
    bindings.endpoint(Role::TsAdapter)={adapter,""};
    bindings.endpoint(Role::TsMux)={mux,""};
    bindings.endpoint(Role::TsMaterializer)={materializer,""};
    bindings.endpoint(Role::Sdp)={rtpSdpPublisher,""};
    bindings.endpoint(Role::TransportPlan)={execution.value().transportPlanSource,""};
    bindings.endpoint(Role::Sender)={execution.value().sender,""};
    if(auto status=bindings.connect(graph,Owner);!status)return Result::failure(status.error());
    std::vector<MediaNodeId> outputMembers{mux, planSource, adapter,
        materializer, execution.value().transportPlanSource, scheduledDatagramSender};
    if (rtpSdpPublisher.isValid()) outputMembers.push_back(rtpSdpPublisher);
    return Result::success(
        {planSource, adapter, mux,
         scheduledDatagramSender, rtpSdpPublisher, std::move(outputMembers),
         std::move(bindings),std::move(execution.value().bindings)});
}

} // namespace

::media::Result<MediaScheduledMpegTsOutputSegmentResult>
MediaScheduledMpegTsOutputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaScheduledMpegTsOutputSegmentOptions& options,
    const MediaRealtimeAvOutputSegmentPlan& plan)
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
                   plan.edgePolicies,
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
                   *output, plan.edgePolicies,
                   plan.datagramTransport});
}

} // namespace media::ffmpeg::graph

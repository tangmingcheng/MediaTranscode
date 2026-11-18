#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaDatagramOutputExecutionSegmentBuilder.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/output/MediaRtpDatagramMaterializerNodePlanCodec.h"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner = "MediaScheduledRtpOutputSegmentBuilder";

::media::Status setSdpOptions(
    MediaGraph& graph,
    MediaNodeId node,
    const std::string& path,
    MediaTranscodeStreamSet streamSet)
{
    auto encodedStreamSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    if (!encodedStreamSet) {
        return ::media::Status::failure(encodedStreamSet.error());
    }
    if (auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, node, "sdp.path", path); !set) {
        return set;
    }
    return MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, node, "sdp.stream_set",
        std::string(encodedStreamSet.value()));
}

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
            "Scheduled RTP output segment received an endpoint with the wrong type"));
    }
    return ::media::Status::success();
}

::media::Status addSenderPorts(
    MediaGraph& graph,
    MediaNodeId node,
    MediaStreamKind stream, MediaPayloadKind codecKind)
{
    using namespace MediaGraphBuildSupport;
    if (auto added = addInputPortChecked(
            graph, Owner, node, "activation", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
        !added) return ::media::Status::failure(added.error());
    if (auto added = addInputPortChecked(
            graph, Owner, node, "codec", stream, MediaEdgeKind::Metadata,
            codecKind, true, false);
        !added) return ::media::Status::failure(added.error());
    if (auto added = addInputPortChecked(
            graph, Owner, node, "scheduled", stream,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet,
            true, false); !added) {
        return ::media::Status::failure(added.error());
    }
    if (auto added = addInputPortChecked(
            graph, Owner, node, "transport_plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata, MediaPayloadKind::DatagramTransportPlan,
            true, false); !added) {
        return ::media::Status::failure(added.error());
    }
    auto added = addOutputPortChecked(
        graph, Owner, node, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    if (!added) return ::media::Status::failure(added.error());
    added = addOutputPortChecked(
        graph, Owner, node, "wire_batch", MediaStreamKind::Metadata,
        MediaEdgeKind::ScheduledDatagramBatch,
        MediaPayloadKind::WireDatagramBatch, true, false);
    return added ? ::media::Status::success()
                 : ::media::Status::failure(added.error());
}

} // namespace

::media::Result<MediaScheduledRtpOutputSegmentResult>
MediaScheduledRtpOutputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaScheduledRtpOutputSegmentOptions& options,
    const MediaRealtimeAvOutputSegmentPlan& plan)
{
    using SegmentResult =
        ::media::Result<MediaScheduledRtpOutputSegmentResult>;
    if (options.prefix.empty() || !plan.groupKey.valid() ||
        plan.outputAdapter !=
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp ||
        !std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(
            plan.protocolOutput)) {
        return SegmentResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP output segment requires its complete planned adapter"));
    }
    for (const auto& [endpoint, stream, edge, payload] : {
             std::tuple{options.epochActivated, MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent},
             std::tuple{options.videoCodec, MediaStreamKind::Video,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext},
             std::tuple{options.audioCodec, MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext},
             std::tuple{options.scheduledVideo, MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet},
             std::tuple{options.scheduledAudio, MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet}}) {
        auto valid = requireSource(graph, endpoint, stream, edge, payload);
        if (!valid) return SegmentResult::failure(valid.error());
    }
    const bool duplicate = std::any_of(
        graph.nodes().begin(), graph.nodes().end(), [](const MediaNode& node) {
            return node.kind == MediaNodeKind::RtpDatagramMaterializer ||
                node.kind == MediaNodeKind::DatagramTransportPlanSource ||
                node.kind == MediaNodeKind::RtpSdpPublisher;
        });
    if (duplicate) {
        return SegmentResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP output segment rejects duplicate output authority"));
    }
    const auto& output =
        std::get<MediaSeparateRtpOutputRuntimePlan>(plan.protocolOutput);
    auto declaration=MediaOutputSegmentPlanner::rtp(true,MediaPayloadKind::CodecContext,plan.edgePolicies, *std::pmr::new_delete_resource());
    if(!declaration)return SegmentResult::failure(declaration.error());
    MediaNodeId video = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::VideoMaterializer),
        options.prefix + ".video.materializer",
        "Video RTP/RTCP protocol materializer");
    MediaNodeId audio = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::AudioMaterializer),
        options.prefix + ".audio.materializer",
        "Audio RTP/RTCP protocol materializer");
    MediaNodeId sdp = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::Sdp),
        options.prefix + ".sdp.publisher",
        "Atomic dual-media SDP publisher");
    if (!video.isValid() || !audio.isValid() || !sdp.isValid()) {
        return SegmentResult::failure(::media::ErrorInfo::internalError(
            "Scheduled RTP output segment failed to add its nodes"));
    }
    auto execution = MediaDatagramOutputExecutionSegmentBuilder::build(
        graph, {options.prefix, options.epochActivated,
                MediaProtocolOutputSessionKey(plan.groupKey.value()),
                MediaTranscodeStreamSet::AudioVideo,
                &plan.datagramTransport, &plan.edgePolicies});
    if (!execution) return SegmentResult::failure(execution.error());
        if (auto applied = MediaRtpDatagramMaterializerNodePlanCodec::apply(
            graph, video,
            MediaProtocolOutputSessionKey(plan.groupKey.value()),
            MediaTranscodeStreamSet::AudioVideo,
            output.video, output.sdp); !applied) {
        return SegmentResult::failure(applied.error());
    }
        if (auto applied = MediaRtpDatagramMaterializerNodePlanCodec::apply(
            graph, audio,
            MediaProtocolOutputSessionKey(plan.groupKey.value()),
            MediaTranscodeStreamSet::AudioVideo,
            output.audio, output.sdp); !applied) {
        return SegmentResult::failure(applied.error());
    }
    if (auto set = setSdpOptions(
            graph, sdp, output.sdp.path,
            MediaTranscodeStreamSet::AudioVideo); !set) {
        return SegmentResult::failure(set.error());
    }
    if (auto added = addSenderPorts(graph, video, MediaStreamKind::Video, MediaPayloadKind::CodecContext);
        !added) return SegmentResult::failure(added.error());
    if (auto added = addSenderPorts(graph, audio, MediaStreamKind::Audio, MediaPayloadKind::CodecContext);
        !added) return SegmentResult::failure(added.error());
    for (const auto& [name, stream] : {
             std::pair{"video", MediaStreamKind::Metadata},
             std::pair{"audio", MediaStreamKind::Metadata}}) {
        auto added = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, sdp, name, stream, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent, true, false);
        if (!added) return SegmentResult::failure(added.error());
    }
    MediaOutputSegmentBindings bindings{std::move(declaration).value(),{}};
    using Role=MediaOutputSegmentRole;
    bindings.endpoint(Role::Activation)=options.epochActivated;
    bindings.endpoint(Role::VideoCodec)=options.videoCodec;
    bindings.endpoint(Role::AudioCodec)=options.audioCodec;
    bindings.endpoint(Role::VideoPacket)=options.scheduledVideo;
    bindings.endpoint(Role::AudioPacket)=options.scheduledAudio;
    bindings.endpoint(Role::VideoMaterializer)={video,""};
    bindings.endpoint(Role::AudioMaterializer)={audio,""};
    bindings.endpoint(Role::Sdp)={sdp,""};
    bindings.endpoint(Role::TransportPlan)={execution.value().transportPlanSource,""};
    bindings.endpoint(Role::Sender)={execution.value().sender,""};
    if(auto status=bindings.connect(graph,Owner);!status)return SegmentResult::failure(status.error());
    return SegmentResult::success({video,audio,sdp,
        {video,audio,sdp,execution.value().transportPlanSource,execution.value().sender},
        std::move(bindings),std::move(execution.value().bindings)});

}

::media::Result<MediaVideoOnlyScheduledRtpOutputSegmentResult>
MediaScheduledRtpOutputSegmentBuilder::buildVideoOnly(
    MediaGraph& graph,
    const MediaVideoOnlyScheduledRtpOutputSegmentOptions& options,
    const MediaRealtimeVideoRuntimePlan& plan)
{
    using Result =
        ::media::Result<MediaVideoOnlyScheduledRtpOutputSegmentResult>;
    const auto* output =
        std::get_if<MediaVideoOnlySeparateRtpOutputRuntimePlan>(
            &plan.outputAdapter);
    if (!output || options.prefix.empty() || !plan.sessionKey.valid()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly RTP output requires its complete runtime product"));
    }
    for (const auto& fact : {
             std::tuple{options.activation, MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent},
             std::tuple{options.videoCodec, MediaStreamKind::Video,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::CodecParameters},
             std::tuple{options.scheduledVideo, MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket,
                        MediaPayloadKind::Packet}}) {
        auto valid = requireSource(
            graph, std::get<0>(fact), std::get<1>(fact),
            std::get<2>(fact), std::get<3>(fact));
        if (!valid) return Result::failure(valid.error());
    }
    if (auto available = MediaDatagramOutputExecutionSegmentBuilder::validateSessionAvailable(
            graph, plan.sessionKey); !available) {
        return Result::failure(available.error());
    }
    auto declaration=MediaOutputSegmentPlanner::rtp(false,MediaPayloadKind::CodecParameters,plan.edgePolicies, *std::pmr::new_delete_resource());
    if(!declaration)return Result::failure(declaration.error());
    const MediaNodeId video = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::VideoMaterializer),
        options.prefix + ".video.materializer", "Video RTP/RTCP protocol materializer");
    const MediaNodeId sdp = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::Sdp),
        options.prefix + ".sdp.publisher", "Atomic video-only SDP publisher");
    if (!video.isValid() || !sdp.isValid()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "VideoOnly RTP output failed to add its nodes"));
    }
    auto execution = MediaDatagramOutputExecutionSegmentBuilder::build(
        graph, {options.prefix, options.activation, plan.sessionKey,
                MediaTranscodeStreamSet::VideoOnly,
                &plan.datagramTransport, &plan.edgePolicies});
    if (!execution) return Result::failure(execution.error());
    auto senderPlan = MediaRtpDatagramMaterializerNodePlanCodec::apply(
        graph, video, plan.sessionKey, MediaTranscodeStreamSet::VideoOnly,
        output->video, output->sdp);
    if (!senderPlan) return Result::failure(senderPlan.error());
    auto sdpOptions = setSdpOptions(
        graph, sdp, output->sdp.path,
        MediaTranscodeStreamSet::VideoOnly);
    if (!sdpOptions) return Result::failure(sdpOptions.error());
    auto senderPorts = addSenderPorts(graph, video, MediaStreamKind::Video, MediaPayloadKind::CodecParameters);
    if (!senderPorts) return Result::failure(senderPorts.error());
    auto sdpPort = MediaGraphBuildSupport::addInputPortChecked(
        graph, Owner, sdp, "video", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    if (!sdpPort) return Result::failure(sdpPort.error());
    MediaOutputSegmentBindings bindings{std::move(declaration).value(),{}};
    using Role=MediaOutputSegmentRole;
    bindings.endpoint(Role::Activation)=options.activation;
    bindings.endpoint(Role::VideoCodec)=options.videoCodec;
    bindings.endpoint(Role::VideoPacket)=options.scheduledVideo;
    bindings.endpoint(Role::VideoMaterializer)={video,""};
    bindings.endpoint(Role::Sdp)={sdp,""};
    bindings.endpoint(Role::TransportPlan)={execution.value().transportPlanSource,""};
    bindings.endpoint(Role::Sender)={execution.value().sender,""};
    if(auto status=bindings.connect(graph,Owner);!status)return Result::failure(status.error());
    return Result::success({video,sdp,std::move(bindings),std::move(execution.value().bindings)});
}

} // namespace media::ffmpeg::graph

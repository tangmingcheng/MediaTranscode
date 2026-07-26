#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner = "MediaScheduledRtpOutputSegmentBuilder";

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
    MediaStreamKind stream)
{
    using namespace MediaGraphBuildSupport;
    if (auto added = addInputPortChecked(
            graph, Owner, node, "epoch", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
        !added) return ::media::Status::failure(added.error());
    if (auto added = addInputPortChecked(
            graph, Owner, node, "codec", stream, MediaEdgeKind::Metadata,
            MediaPayloadKind::CodecContext, true, false);
        !added) return ::media::Status::failure(added.error());
    if (auto added = addInputPortChecked(
            graph, Owner, node, "scheduled", stream,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet,
            true, false); !added) {
        return ::media::Status::failure(added.error());
    }
    auto added = addOutputPortChecked(
        graph, Owner, node, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    return added ? ::media::Status::success()
                 : ::media::Status::failure(added.error());
}

} // namespace

::media::Result<MediaScheduledRtpOutputSegmentResult>
MediaScheduledRtpOutputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaScheduledRtpOutputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
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
            return node.kind == MediaNodeKind::ScheduledRtpSender ||
                node.kind == MediaNodeKind::DualMediaSdpPublisher;
        });
    if (duplicate) {
        return SegmentResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP output segment rejects duplicate output authority"));
    }
    const auto& output =
        std::get<MediaSeparateRtpOutputRuntimePlan>(plan.protocolOutput);
    MediaNodeId video = graph.addNode(
        MediaNodeKind::ScheduledRtpSender,
        options.prefix + ".video.sender",
        "Scheduled video RTP/RTCP sender");
    MediaNodeId audio = graph.addNode(
        MediaNodeKind::ScheduledRtpSender,
        options.prefix + ".audio.sender",
        "Scheduled audio RTP/RTCP sender");
    MediaNodeId sdp = graph.addNode(
        MediaNodeKind::DualMediaSdpPublisher,
        options.prefix + ".sdp.publisher",
        "Atomic dual-media SDP publisher");
    if (!video.isValid() || !audio.isValid() || !sdp.isValid()) {
        return SegmentResult::failure(::media::ErrorInfo::internalError(
            "Scheduled RTP output segment failed to add its nodes"));
    }
    if (auto applied = MediaScheduledRtpSenderNodePlanCodec::apply(
            graph, video, plan.groupKey, output.video, output.sdp); !applied) {
        return SegmentResult::failure(applied.error());
    }
    if (auto applied = MediaScheduledRtpSenderNodePlanCodec::apply(
            graph, audio, plan.groupKey, output.audio, output.sdp); !applied) {
        return SegmentResult::failure(applied.error());
    }
    if (auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, sdp, "sdp.path", output.sdp.path); !set) {
        return SegmentResult::failure(set.error());
    }
    if (auto added = addSenderPorts(graph, video, MediaStreamKind::Video);
        !added) return SegmentResult::failure(added.error());
    if (auto added = addSenderPorts(graph, audio, MediaStreamKind::Audio);
        !added) return SegmentResult::failure(added.error());
    for (const auto& [name, stream] : {
             std::pair{"video", MediaStreamKind::Metadata},
             std::pair{"audio", MediaStreamKind::Metadata}}) {
        auto added = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, sdp, name, stream, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent, true, false);
        if (!added) return SegmentResult::failure(added.error());
    }
    const auto connect = [&](const MediaEndpoint& from,
                             MediaNodeId to,
                             const char* port,
                             const char* label,
                             const MediaEdgePolicy& policy) {
        return MediaGraphBuildSupport::connectChecked(
            graph, Owner, from.node, from.port, to, port, label, policy);
    };
    for (MediaNodeId sender : {video, audio}) {
        auto connected = connect(
            options.epochActivated, sender, "epoch",
            "playback epoch -> scheduled RTP sender",
            plan.edgePolicies.atomicMetadata);
        if (!connected) return SegmentResult::failure(connected.error());
    }
    for (const auto& [from, to, port, label, policy] : {
             std::tuple{options.videoCodec, video, "codec",
                        "video codec -> scheduled RTP sender",
                        plan.edgePolicies.metadata},
             std::tuple{options.audioCodec, audio, "codec",
                        "audio codec -> scheduled RTP sender",
                        plan.edgePolicies.metadata},
             std::tuple{options.scheduledVideo, video, "scheduled",
                        "scheduled video -> RTP sender",
                        plan.edgePolicies.atomicVideoPacket},
             std::tuple{options.scheduledAudio, audio, "scheduled",
                        "scheduled audio -> RTP sender",
                        plan.edgePolicies.atomicAudioPacket}}) {
        auto connected = connect(from, to, port, label, policy);
        if (!connected) return SegmentResult::failure(connected.error());
    }
    for (const auto& [from, port, label] : {
             std::tuple{video, "video", "video sender -> SDP publisher"},
             std::tuple{audio, "audio", "audio sender -> SDP publisher"}}) {
        auto connected = MediaGraphBuildSupport::connectChecked(
            graph, Owner, from, "description", sdp, port, label,
            plan.edgePolicies.metadata);
        if (!connected) return SegmentResult::failure(connected.error());
    }
    return SegmentResult::success(
        MediaScheduledRtpOutputSegmentResult{video, audio, sdp});
}

} // namespace media::ffmpeg::graph

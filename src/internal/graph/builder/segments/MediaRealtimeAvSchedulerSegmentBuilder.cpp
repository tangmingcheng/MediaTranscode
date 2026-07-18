#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include <algorithm>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner = "MediaRealtimeAvSchedulerSegmentBuilder";

::media::Result<void> validateSource(
    const MediaGraph& graph,
    const MediaEndpoint& endpoint,
    MediaStreamKind stream)
{
    if (!endpoint.valid() || !graph.findNode(endpoint.node)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler segment requires complete canonical endpoints"));
    }
    const MediaPort* port = graph.findOutputPort(endpoint.node, endpoint.port);
    if (!port || port->streamKind != stream ||
        port->edgeKind != MediaEdgeKind::EncodedPacket ||
        port->payloadKind != MediaPayloadKind::Packet) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler segment canonical endpoint has the wrong type"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validatePlan(
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const auto& queue = plan.edgePolicies.synchronizedPacket.queuePolicy;
    if (!plan.groupKey.valid() || !queue.bounded || queue.capacity == 0 ||
        queue.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer ||
        queue.orderingPolicy != MediaQueueOrderingPolicy::Fifo ||
        !queue.preserveOrdering) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler segment requires its planned sync group and ordered packet policy"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> addPorts(
    MediaGraph& graph,
    MediaNodeId scheduler,
    MediaNodeId router)
{
    using namespace MediaGraphBuildSupport;
    if (auto status = addInputPortChecked(
            graph, Owner, scheduler, "video", MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true,
            false); !status) return status;
    if (auto status = addInputPortChecked(
            graph, Owner, scheduler, "audio", MediaStreamKind::Audio,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true,
            false); !status) return status;
    if (auto status = addOutputPortChecked(
            graph, Owner, scheduler, "scheduled", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true,
            false); !status) return status;
    if (auto status = addInputPortChecked(
            graph, Owner, router, "scheduled", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true,
            false); !status) return status;
    if (auto status = addOutputPortChecked(
            graph, Owner, router, "video", MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true,
            false); !status) return status;
    return addOutputPortChecked(
        graph, Owner, router, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, false);
}

} // namespace

::media::Result<MediaRealtimeAvSchedulerSegmentResult>
MediaRealtimeAvSchedulerSegmentBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeAvSchedulerSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (options.prefix.empty()) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler segment requires a planned prefix"));
    }
    if (auto status = validateSource(
            graph, options.canonicalVideo, MediaStreamKind::Video); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    if (auto status = validateSource(
            graph, options.canonicalAudio, MediaStreamKind::Audio); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    if (auto status = validatePlan(plan); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    const bool alreadyAssembled = std::any_of(
        graph.nodes().begin(), graph.nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::AvOutputScheduler ||
                node.kind == MediaNodeKind::ScheduledOutputRouter;
        });
    if (alreadyAssembled) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler segment rejects duplicate scheduling authority"));
    }
    MediaNodeId scheduler = graph.addNode(
        MediaNodeKind::AvOutputScheduler, options.prefix + ".scheduler",
        "Shared A/V output scheduler");
    MediaNodeId router = graph.addNode(
        MediaNodeKind::ScheduledOutputRouter, options.prefix + ".router",
        "Typed scheduled output router");
    if (!scheduler.isValid() || !router.isValid()) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler segment failed to add its nodes"));
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, scheduler, "av_scheduler.sync_group",
            plan.groupKey.value()); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    if (auto status = addPorts(graph, scheduler, router); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    const auto& policy = plan.edgePolicies.synchronizedPacket;
    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, Owner, options.canonicalVideo.node,
            options.canonicalVideo.port, scheduler, "video",
            "canonical video -> shared scheduler", policy); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, Owner, options.canonicalAudio.node,
            options.canonicalAudio.port, scheduler, "audio",
            "canonical audio -> shared scheduler", policy); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, Owner, scheduler, "scheduled", router, "scheduled",
            "shared scheduler -> typed router", policy); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::success(
        MediaRealtimeAvSchedulerSegmentResult{
            MediaEndpoint{router, "video"}, MediaEndpoint{router, "audio"}});
}

} // namespace media::ffmpeg::graph

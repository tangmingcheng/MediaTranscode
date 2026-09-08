#include "internal/graph/builder/segments/MediaRealtimeVideoSchedulerSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <algorithm>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner =
    "MediaRealtimeVideoSchedulerSegmentBuilder";

} // namespace

::media::Result<MediaRealtimeVideoSchedulerSegmentResult>
MediaRealtimeVideoSchedulerSegmentBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeVideoSchedulerSegmentOptions& options,
    const MediaRealtimeVideoRuntimePlan& plan)
{
    if (options.prefix.empty() || !options.encodedVideo.valid() ||
        !graph.findNode(options.encodedVideo.node)) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler segment requires its planned endpoint and prefix"));
    }
    const MediaPort* source = graph.findOutputPort(
        options.encodedVideo.node, options.encodedVideo.port);
    if (!source || source->streamKind != MediaStreamKind::Video ||
        source->edgeKind != MediaEdgeKind::EncodedPacket ||
        source->payloadKind != MediaPayloadKind::Packet) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler source has the wrong type"));
    }
    const bool schedulingAuthorityExists = std::any_of(
        graph.nodes().begin(), graph.nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::VideoOutputScheduler ||
                node.kind == MediaNodeKind::AvOutputScheduler ||
                node.kind == MediaNodeKind::ScheduledOutputRouter;
        });
    if (schedulingAuthorityExists) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler rejects duplicate scheduling authority"));
    }
    const auto& queue = plan.edgePolicies.synchronizedPacket.queuePolicy;
    if (!queue.bounded || queue.capacity == 0 ||
        queue.overflowPolicy != MediaQueueOverflowPolicy::BlockProducer ||
        queue.orderingPolicy != MediaQueueOrderingPolicy::Fifo ||
        !queue.preserveOrdering) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler requires its planner-owned ordered queue"));
    }
    const MediaNodeId scheduler = graph.addNode(
        MediaNodeKind::VideoOutputScheduler,
        options.prefix + ".scheduler",
        "Planner-owned VideoOnly output scheduler");
    if (!scheduler.isValid()) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::internalError(
                "VideoOnly scheduler segment failed to add its node"));
    }
    const auto set = [&](const char* key, std::string value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, scheduler, key, std::move(value));
    };
    if (auto status = set(
            "video_scheduler.startup.require_key_frame",
            plan.startup.requireKeyFrame ? "1" : "0"); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.startup.maximum_wait_ns",
            std::to_string(plan.startup.maximumWait.nanoseconds())); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.startup.packet_capacity",
            std::to_string(plan.startup.packetCapacity)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.startup.maximum_unit_bytes",
            std::to_string(plan.startup.maximumUnitBytes)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.startup.byte_capacity",
            std::to_string(plan.startup.byteCapacity)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.source_time_base.num",
            std::to_string(plan.timing.sourceTimeBase.num)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.source_time_base.den",
            std::to_string(plan.timing.sourceTimeBase.den)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.output_frame_rate.num",
            std::to_string(plan.timing.outputFrameRate.num)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.output_frame_rate.den",
            std::to_string(plan.timing.outputFrameRate.den)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.packet_time_base.num",
            std::to_string(plan.timing.scheduledPacketTimeBase.num)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.packet_time_base.den",
            std::to_string(plan.timing.scheduledPacketTimeBase.den)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    const char* timingMode = plan.timing.packetTimingMode ==
            MediaRealtimeVideoPacketTimingMode::PacketDuration
        ? "packet_duration"
        : plan.timing.packetTimingMode ==
                MediaRealtimeVideoPacketTimingMode::PlannedCadence
            ? "planned_cadence"
            : nullptr;
    if (!timingMode) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler has an unknown packet timing mode"));
    }
    if (auto status = set(
            "video_scheduler.packet_timing_mode", timingMode); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.activation_lead_ns",
            std::to_string(plan.scheduling.activationLead.nanoseconds()));
        !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.transport_lead_ns",
            std::to_string(plan.scheduling.transportLead.nanoseconds()));
        !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.protocol_preparation_lead_ns",
            std::to_string(
                plan.scheduling.protocolPreparationLead.nanoseconds()));
        !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.pacing_enabled",
            plan.scheduling.pacingEnabled ? "1" : "0"); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "video_scheduler.initial_generation",
            std::to_string(plan.scheduling.initialGeneration)); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = set(
            "protocol_output.session", plan.sessionKey.value()); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, scheduler, "video", MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true,
            false); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
            graph, Owner, scheduler, "activation",
            MediaStreamKind::Metadata, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent, true, true); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
            graph, Owner, scheduler, "scheduled_video",
            MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
            MediaPayloadKind::Packet, true, false); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, Owner, options.encodedVideo.node,
            options.encodedVideo.port, scheduler, "video",
            "encoded video -> VideoOnly scheduler",
            plan.edgePolicies.synchronizedPacket); !status) {
        return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::failure(status.error());
    }
    return ::media::Result<MediaRealtimeVideoSchedulerSegmentResult>::success(
        MediaRealtimeVideoSchedulerSegmentResult{
            MediaEndpoint{scheduler, "activation"},
            MediaEndpoint{scheduler, "scheduled_video"}});
}

} // namespace media::ffmpeg::graph

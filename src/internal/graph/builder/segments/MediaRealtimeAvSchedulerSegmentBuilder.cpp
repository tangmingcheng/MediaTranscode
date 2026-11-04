#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include <algorithm>
#include <string_view>
#include <utility>

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
    const MediaRealtimeAvOutputSegmentPlan& plan)
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
    if (plan.audioBranchMode == MediaBranchMode::CopyPacket &&
        !MediaAtomicOutputPolicyContract::accepts(
            plan.canonicalAudioInputPolicy)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler packet-copy audio requires its planner atomic release policy"));
    }
    if (plan.audioBranchMode == MediaBranchMode::TranscodeFrame &&
        plan.canonicalAudioInputPolicy != plan.edgePolicies.synchronizedPacket)
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Encoded scheduler audio input differs from its output retention product"));
    return ::media::Result<void>::success();
}

::media::Result<void> addPorts(
    MediaGraph& graph,
    MediaNodeId scheduler,
    MediaNodeId router,
    MediaAvSyncOutputAdapterKind outputAdapter)
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
    if (outputAdapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        return addOutputPortChecked(
            graph, Owner, router, "serialized", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, false);
    }
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
    const MediaRealtimeAvOutputSegmentPlan& plan)
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
    auto declaration=MediaOutputSegmentPlanner::scheduler(plan.edgePolicies,plan.canonicalAudioInputPolicy);
    if(!declaration)return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(declaration.error());
    MediaNodeId scheduler = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::Scheduler), options.prefix + ".scheduler",
        "Shared A/V output scheduler");
    MediaNodeId router = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::Router), options.prefix + ".router",
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
    if (auto status = plan.scheduling.validate(plan.protocolOutput,
            plan.scheduling.protocolPreparationLead); !status)
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(status.error());
    for (const auto& [key, value] : {
             std::pair{"av_scheduler.transport_lead_ns", std::to_string(plan.scheduling.transportLead.nanoseconds())},
             std::pair{"av_scheduler.protocol_preparation_lead_ns", std::to_string(plan.scheduling.protocolPreparationLead.nanoseconds())},
             std::pair{"av_scheduler.submission", std::string("encoded_access_unit")}}) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, Owner, scheduler, key, value); !status)
            return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(status.error());
    }
    const char* routerMode =
        plan.outputAdapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs
        ? "serialized_av" : "split_av";
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, router, "scheduled_output_router.mode", routerMode);
        !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    if (auto status = addPorts(
            graph, scheduler, router, plan.outputAdapter); !status) {
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(
            status.error());
    }
    MediaOutputSegmentBindings bindings{std::move(declaration).value(),{}};
    bindings.endpoint(MediaOutputSegmentRole::VideoPacket)=options.canonicalVideo;
    bindings.endpoint(MediaOutputSegmentRole::AudioPacket)=options.canonicalAudio;
    bindings.endpoint(MediaOutputSegmentRole::Scheduler)={scheduler,""};
    bindings.endpoint(MediaOutputSegmentRole::Router)={router,""};
    if(auto status=bindings.connect(graph,Owner);!status)
        return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::failure(status.error());
    MediaRealtimeAvSchedulerSegmentResult result;
    result.bindings=std::move(bindings);
    result.scheduler = scheduler;
    result.outputMembers = {scheduler, router};
    if (plan.outputAdapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        result.serialized = MediaEndpoint{router, "serialized"};
    } else {
        result.video = MediaEndpoint{router, "video"};
        result.audio = MediaEndpoint{router, "audio"};
    }
    return ::media::Result<MediaRealtimeAvSchedulerSegmentResult>::success(
        std::move(result));
}

} // namespace media::ffmpeg::graph

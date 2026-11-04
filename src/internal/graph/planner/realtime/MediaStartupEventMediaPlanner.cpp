#include "internal/graph/planner/realtime/MediaStartupEventMediaPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRouteMediaPlanner.h"

#include <algorithm>
#include <new>
#include <tuple>

namespace media::ffmpeg::graph {

::media::Result<std::vector<MediaStartupEventMediaBound>> MediaStartupEventMediaPlanner::plan(
    const MediaGraph& graph, const MediaStartupBatchPayloadPlan& batch)
try {
    using Result = ::media::Result<std::vector<MediaStartupEventMediaBound>>;
    const auto invalid = [](const char* text) { return Result::failure(::media::ErrorInfo::invalidArgument(text)); };
    const auto* coordinator = graph.findNode(batch.coordinator);
    if (!coordinator) return invalid("Startup media route has no coordinator");
    auto current = MediaStartupBatchPayloadPlanner::plan(*coordinator);
    if (!current) return Result::failure(current.error());
    if (current.value().videoPayload != batch.videoPayload || current.value().videoUnits != batch.videoUnits ||
        current.value().audioPacketUnits != batch.audioPacketUnits)
        return invalid("Startup media route batch differs from its coordinator");
    auto readiness = decodeMediaVideoStartupReadiness(coordinator->options.value("av_startup.video_readiness"));
    if (!readiness) return Result::failure(readiness.error());
    auto shape = MediaStartupRoutePlanner::describe(batch.audioPacketUnits
        ? MediaTranscodeStreamSet::AudioVideo : MediaTranscodeStreamSet::VideoOnly, readiness.value());
    if (!shape) return Result::failure(shape.error());
    std::array<std::size_t, std::tuple_size_v<MediaStartupRouteShape::Connections>> connectionCounts{};
    const MediaEdgePolicy* metadata = nullptr;
    const MediaEdgePolicy* atomicMetadata = nullptr;
    const auto group = coordinator->options.value("av_startup.sync_group");
    if (group.empty()) return invalid("Startup media route requires a source group");
    std::vector<MediaStartupEventMediaBound> result;
    const auto add = [&](const MediaEdge& edge, MediaStartupEventMediaForm form) -> ::media::Status {
        const auto* from = graph.findPort(edge.from.portId);
        const auto* to = graph.findPort(edge.to.portId);
        const auto event = [](const MediaPort* port) {
            return port && port->streamKind == MediaStreamKind::Metadata &&
                port->edgeKind == MediaEdgeKind::Event && port->payloadKind == MediaPayloadKind::GraphEvent;
        };
        if (!event(from) || !event(to) || from->nodeId != edge.from.nodeId || to->nodeId != edge.to.nodeId ||
            from->direction != MediaPortDirection::Output || to->direction != MediaPortDirection::Input ||
            edge.streamKind != MediaStreamKind::Metadata || edge.payloadKind != MediaPayloadKind::GraphEvent ||
            edge.edgeKind != MediaEdgeKind::Event || !edge.policy.queuePolicy.bounded || !edge.policy.queuePolicy.capacity)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup media route requires a bounded event edge"));
        if (std::any_of(result.begin(), result.end(), [&](const auto& value) { return value.edge == edge.id; }))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup media route repeats an edge"));
        const auto* source = graph.findNode(edge.from.nodeId);
        const auto* target = graph.findNode(edge.to.nodeId);
        const auto* connection = source && target
            ? shape.value().find(source->kind, from->name, target->kind, to->name) : nullptr;
        if (!connection || connection->form != form)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup edge differs from its typed route role"));
        auto*& observedPolicy = connection->policy == MediaStartupRoutePolicy::Metadata ? metadata : atomicMetadata;
        if (observedPolicy && *observedPolicy != edge.policy)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup route has inconsistent policies for the same queue role"));
        observedPolicy = &edge.policy;
        ++connectionCounts[static_cast<std::size_t>(connection - shape.value().connections().data())];
        result.push_back({edge.id, batch.coordinator, form, batch.videoPayload, std::nullopt, std::nullopt,
            static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity)});
        return ::media::Status::success();
    };
    std::size_t videos = 0, audios = 0;
    for (const auto& edge : graph.edges()) {
        if (edge.to.nodeId != coordinator->id) continue;
        const auto* input = graph.findPort(edge.to.portId);
        const auto* output = graph.findPort(edge.from.portId);
        const auto* source = graph.findNode(edge.from.nodeId);
        if (!input || !output || !source) return invalid("Startup media input endpoint is missing");
        if (input->name == "clock") continue;
        const bool video = input->name == "video";
        const auto* connection = shape.value().find(source->kind, output->name, coordinator->kind, input->name);
        if (!connection || (connection->form != MediaStartupEventMediaForm::VideoEnvelope &&
            connection->form != MediaStartupEventMediaForm::AudioEnvelope))
            return invalid("Startup media input does not match its envelope producer");
        const auto identity = source->options.value(source->kind == MediaNodeKind::DecodedVideoStartup
            ? "decoded_video_startup.source_identity" : "canonical_input.source_identity");
        if (identity.empty() || identity != coordinator->options.value(video ? "av_startup.video_identity" : "av_startup.audio_identity") ||
            (source->kind == MediaNodeKind::CanonicalInput &&
             (source->options.value("canonical_input.sync_group") != group ||
              source->options.value("canonical_input.stream") != (video ? "video" : "audio"))))
            return invalid("Startup envelope belongs to a different source or stream");
        auto status = add(edge, video ? MediaStartupEventMediaForm::VideoEnvelope : MediaStartupEventMediaForm::AudioEnvelope);
        if (!status) return Result::failure(status.error());
        if (video) ++videos; else ++audios;
    }
    if (videos != 1 || audios != (batch.audioPacketUnits ? 1u : 0u))
        return invalid("Startup media envelope inputs differ from planned members");
    std::vector<MediaNodeId> pending{coordinator->id};
    for (std::size_t index = 0; index < pending.size(); ++index) {
        const auto* node = graph.findNode(pending[index]);
        if (!node) return invalid("Startup media route node is missing");
        const char* groupKey = node->kind == MediaNodeKind::AvStartupCoordinator ? "av_startup.sync_group"
            : node->kind == MediaNodeKind::PlaybackEpochBinder ? "playback_epoch_binder.sync_group"
            : node->kind == MediaNodeKind::ActivatedStartupReleaseSequencer ? "activated_startup_release_sequencer.sync_group"
            : "av_bound_release_extractor.sync_group";
        if (node->options.value(groupKey) != group) return invalid("Startup media route crosses source groups");
        if (node->kind == MediaNodeKind::AvBoundReleaseExtractor) continue;
        std::size_t carryingOutputs = 0;
        for (const auto& edge : graph.edges()) {
            if (edge.from.nodeId != node->id) continue;
            const auto* from = graph.findPort(edge.from.portId);
            const auto* to = graph.findPort(edge.to.portId);
            const auto* target = graph.findNode(edge.to.nodeId);
            if (!from || !to || !target) return invalid("Startup media route endpoint is missing");
            if (node->kind == MediaNodeKind::ActivatedStartupReleaseSequencer && from->name == "activated") continue;
            const auto* connection = shape.value().find(node->kind, from->name, target->kind, to->name);
            if (!connection) return invalid("Startup media route has an unsupported payload-carrying transfer");
            auto status = add(edge, connection->form);
            if (!status) return Result::failure(status.error());
            ++carryingOutputs;
            if (std::find(pending.begin(), pending.end(), target->id) == pending.end()) pending.push_back(target->id);
        }
        const auto expectedOutputs = std::count_if(shape.value().connections().begin(),
            shape.value().connections().end(), [&](const auto& connection) {
                return shape.value().includes(connection) && shape.value().nodeKind(connection.from) == node->kind;
            });
        if (carryingOutputs != static_cast<std::size_t>(expectedOutputs))
            return invalid("Startup media route has incomplete or duplicate payload outputs");
    }
    if (std::count_if(pending.begin(), pending.end(), [&](auto id) {
            return graph.findNode(id)->kind == MediaNodeKind::AvBoundReleaseExtractor; }) != 1)
        return invalid("Startup preparation and bound release require the same extractor");
    if (!metadata || !atomicMetadata) return invalid("Startup route lacks its two explicit queue-policy roles");
    auto route = MediaStartupRoutePlanner::bind(shape.value(), *metadata, *atomicMetadata);
    if (!route) return Result::failure(route.error());
    for (std::size_t i = 0; i != connectionCounts.size(); ++i)
        if (connectionCounts[i] != (route.value().shape.includes(route.value().shape.connections()[i]) ? 1u : 0u))
            return invalid("Startup graph does not realize every planned route connection exactly once");
    auto media = MediaStartupRouteMediaPlanner::plan(route.value(), batch.videoUnits, batch.audioPacketUnits);
    if (!media) return Result::failure(media.error());
    if (media.value().videoPayload != batch.videoPayload) return invalid("Startup route payload differs from readiness");
    for (auto& event : result) {
        const auto* edge = graph.findEdge(event.edge);
        const auto* connection = route.value().shape.find(graph.findNode(edge->from.nodeId)->kind,
            graph.findPort(edge->from.portId)->name, graph.findNode(edge->to.nodeId)->kind,
            graph.findPort(edge->to.portId)->name);
        if (!connection) return invalid("Validated startup connection disappeared");
        const auto& units = media.value().events[static_cast<std::size_t>(connection - route.value().shape.connections().data())];
        if (!units) return invalid("Validated startup connection has no media product");
        event.videoUnits = units->media.video;
        event.audioPacketUnits = units->media.audio;
        event.wrapperSlots = units->wrapperSlots;
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::vector<MediaStartupEventMediaBound>>::failure(
        ::media::ErrorInfo::allocationFailed("Startup event media planning allocation failed"));
}

} // namespace media::ffmpeg::graph

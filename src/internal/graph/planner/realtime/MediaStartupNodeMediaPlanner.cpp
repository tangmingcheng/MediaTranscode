#include "internal/graph/planner/realtime/MediaStartupNodeMediaPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRouteMediaPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
::media::Result<std::vector<MediaStartupNodeMediaBound>> MediaStartupNodeMediaPlanner::plan(
    const MediaGraph& graph, const MediaStartupBatchPayloadPlan& batch,
    std::span<const MediaStartupEventMediaBound> events)
try {
    using R = ::media::Result<std::vector<MediaStartupNodeMediaBound>>;
    const auto invalid = [](const char* text) { return R::failure(::media::ErrorInfo::invalidArgument(text)); };
    // Reuse the complete graph/identity validation, including exact connection
    // multiplicities. Supplied event products must describe that same graph.
    auto checked = MediaStartupEventMediaPlanner::plan(graph, batch);
    if (!checked) return R::failure(checked.error());
    if (checked.value().size() != events.size()) return invalid("Startup node media event count differs from graph");
    for (const auto& expected : checked.value()) {
        if (std::count_if(events.begin(), events.end(), [&](const auto& actual) {
            return actual.edge == expected.edge && actual.coordinator == expected.coordinator &&
                actual.form == expected.form && actual.videoPayload == expected.videoPayload &&
                actual.videoUnits == expected.videoUnits && actual.audioPacketUnits == expected.audioPacketUnits &&
                actual.wrapperSlots == expected.wrapperSlots;
        }) != 1) return invalid("Startup node media events differ from validated graph");
    }
    const auto* coordinator = graph.findNode(batch.coordinator);
    auto readiness = decodeMediaVideoStartupReadiness(coordinator->options.value("av_startup.video_readiness"));
    if (!readiness) return R::failure(readiness.error());
    auto shape = MediaStartupRoutePlanner::describe(batch.audioPacketUnits
        ? MediaTranscodeStreamSet::AudioVideo : MediaTranscodeStreamSet::VideoOnly, readiness.value());
    if (!shape) return R::failure(shape.error());
    const MediaEdgePolicy* metadata = nullptr;
    const MediaEdgePolicy* atomicMetadata = nullptr;
    std::array<std::optional<MediaNodeId>, static_cast<std::size_t>(MediaStartupRouteRole::Extractor) + 1> owners{};
    for (const auto& event : events) {
        const auto* edge = graph.findEdge(event.edge);
        const auto* connection = shape.value().find(graph.findNode(edge->from.nodeId)->kind,
            graph.findPort(edge->from.portId)->name, graph.findNode(edge->to.nodeId)->kind,
            graph.findPort(edge->to.portId)->name);
        if (!connection) return invalid("Startup node media route is missing");
        auto*& policy = connection->policy == MediaStartupRoutePolicy::Metadata ? metadata : atomicMetadata;
        policy = &edge->policy;
        const auto bind = [&](MediaStartupRouteRole role, MediaNodeId id) {
            auto& owner = owners[static_cast<std::size_t>(role)];
            if (owner && *owner != id) return false;
            owner = id; return true;
        };
        if (!bind(connection->from, edge->from.nodeId) || !bind(connection->to, edge->to.nodeId))
            return invalid("Startup role maps to multiple node owners");
    }
    if (!metadata || !atomicMetadata) return invalid("Startup node route lacks queue roles");
    auto route = MediaStartupRoutePlanner::bind(shape.value(), *metadata, *atomicMetadata);
    if (!route) return R::failure(route.error());
    auto media = MediaStartupRouteMediaPlanner::plan(route.value(), batch.videoUnits, batch.audioPacketUnits);
    if (!media) return R::failure(media.error());
    std::vector<MediaStartupNodeMediaBound> result;
    for (std::size_t role = 0; role < media.value().nodes.size(); ++role) {
        const auto& units = media.value().nodes[role];
        if (!units) continue;
        if (!owners[role]) return invalid("Startup retained role has no graph owner");
        result.push_back({*owners[role], batch.coordinator, media.value().videoPayload, units->video, units->audio});
    }
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::vector<MediaStartupNodeMediaBound>>::failure(
        ::media::ErrorInfo::allocationFailed("Startup node media planning allocation failed"));
}
} // namespace media::ffmpeg::graph

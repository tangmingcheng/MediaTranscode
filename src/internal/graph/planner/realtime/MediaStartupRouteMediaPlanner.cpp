#include "internal/graph/planner/realtime/MediaStartupRouteMediaPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include <limits>
namespace media::ffmpeg::graph {
::media::Result<MediaStartupRouteMediaPlan> MediaStartupRouteMediaPlanner::plan(
    const MediaStartupRoutePlan& route, std::uint64_t video,
    std::optional<std::uint64_t> audio)
{
    using R = ::media::Result<MediaStartupRouteMediaPlan>;
    auto valid = MediaStartupRoutePlanner::bind(route.shape, route.metadata, route.atomicMetadata);
    if (!valid) return R::failure(valid.error());
    const auto maximum = static_cast<std::uint64_t>((std::numeric_limits<int>::max)());
    if (!video || video > maximum ||
        audio.has_value() != (route.shape.members == MediaTranscodeStreamSet::AudioVideo) ||
        (audio && (!*audio || *audio > maximum)))
        return R::failure(::media::ErrorInfo::invalidArgument("Startup route capacities require positive-int member bounds"));
    MediaStartupRouteMediaPlan result{};
    result.videoPayload = route.shape.readiness == MediaVideoStartupReadiness::DecodedPresentationSpan
        ? MediaPayloadKind::Frame : MediaPayloadKind::Packet;
    // The positive-int inputs make these uint64 additions representable.
    result.nodes[static_cast<std::size_t>(MediaStartupRouteRole::Coordinator)] =
        MediaStartupRouteMediaUnits{video + 1, audio ? std::optional{*audio + 1} : std::nullopt};
    const auto& connections = route.shape.connections();
    for (std::size_t i = 0; i < connections.size(); ++i) {
        const auto& connection = connections[i];
        if (!route.shape.includes(connection)) continue;
        MediaStartupRouteMediaUnits units{video, audio};
        auto owner = connection.to;
        if (connection.form == MediaStartupEventMediaForm::VideoEnvelope) {
            units = {1, std::nullopt}; owner = connection.from;
        } else if (connection.form == MediaStartupEventMediaForm::AudioEnvelope) {
            units = {std::nullopt, 1}; owner = connection.from;
        }
        result.events[i] = MediaStartupRouteEventUnits{connection.form, units,
            static_cast<std::uint64_t>(route.policy(connection).queuePolicy.capacity)};
        auto& retained = result.nodes[static_cast<std::size_t>(owner)];
        if (retained && *retained != units)
            return R::failure(::media::ErrorInfo::invalidArgument("Startup route has conflicting role retention"));
        retained = units;
    }
    return R::success(std::move(result));
}
} // namespace media::ffmpeg::graph

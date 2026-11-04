#pragma once
#include "internal/graph/model/MediaStartupRoutePlan.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "media_transcode/Result.h"
#include <cstdint>
#include <optional>
#include <tuple>
namespace media::ffmpeg::graph {
struct MediaStartupRouteMediaUnits final {
    std::optional<std::uint64_t> video;
    std::optional<std::uint64_t> audio;
    bool operator==(const MediaStartupRouteMediaUnits&) const = default;
};
struct MediaStartupRouteEventUnits final {
    MediaStartupEventMediaForm form;
    MediaStartupRouteMediaUnits media;
    std::uint64_t wrapperSlots;
};
struct MediaStartupRouteMediaPlan final {
    MediaPayloadKind videoPayload;
    std::array<std::optional<MediaStartupRouteEventUnits>,
        std::tuple_size_v<MediaStartupRouteShape::Connections>> events;
    // Existing roles are contiguous through Extractor; no second route table.
    std::array<std::optional<MediaStartupRouteMediaUnits>,
        static_cast<std::size_t>(MediaStartupRouteRole::Extractor) + 1> nodes;
};
class MediaStartupRouteMediaPlanner final {
public:
    static ::media::Result<MediaStartupRouteMediaPlan> plan(
        const MediaStartupRoutePlan& route, std::uint64_t videoBatchUnits,
        std::optional<std::uint64_t> audioBatchUnits);
};
} // namespace media::ffmpeg::graph

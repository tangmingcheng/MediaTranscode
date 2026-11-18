#pragma once
#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"
#include <cstddef>

namespace media::ffmpeg::graph {
struct MediaTsSeriesOwnershipPlan final {
    std::size_t muxQueuedSeries;
    std::size_t muxPendingMediaSeries;
    std::size_t muxMaintenanceWorkingSeries;
    std::size_t muxPublicationSeries;
    std::size_t protocolEdgeSeries;
    std::size_t materializerCurrentSeries;
    std::size_t globallyReservedSeries;
    std::size_t senderCommitRetirementSeries;
    ::media::Result<std::size_t> total() const;
    ::media::Result<std::size_t> prefixViews() const;
};
class MediaTsSeriesOwnershipPlanner final {
public:
    static ::media::Result<MediaTsSeriesOwnershipPlan> plan(const MediaGraph& graph,
        MediaNodeId transportSource, MediaNodeId mux, std::size_t muxQueueSlots,
        std::size_t maximumOutstandingDatagrams);
};
} // namespace media::ffmpeg::graph

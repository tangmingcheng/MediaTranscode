#pragma once

#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

namespace media::ffmpeg::graph {

class MediaPreparedRtpStorageAdmissionPlanner final {
public:
    static ::media::Result<MediaFinalGraphResourceLedgerEntry> plan(
        const MediaGraph& graph, const MediaPreparedRtpStorageReservation& reservation,
        std::span<const MediaNodeId> owners, std::size_t allocationIndex);
};

} // namespace media::ffmpeg::graph

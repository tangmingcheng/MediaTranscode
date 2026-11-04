#pragma once
#include "internal/graph/planner/realtime/MediaTsAcceptedWorkService.h"
#include "internal/graph/planner/realtime/MediaTsPrefixServicePlan.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

namespace media::ffmpeg::graph {
class MediaTsAcceptedWorkPlanner final {
public:
    static ::media::Result<MediaTsPublishedWorkService> plan(
        std::span<MediaTsAcceptedReservationObservation> records,
        const MediaWireGlobalSequenceSnapshot& wire, std::uint64_t generation,
        MediaRunningTime observedNow, const MediaTsPrefixServicePlan& service);
    static ::media::Status validateTail(const MediaTsUnpublishedTail& tail,
        MediaRunningTime availableAt, const MediaTsPrefixServicePlan& service);
};
} // namespace media::ffmpeg::graph

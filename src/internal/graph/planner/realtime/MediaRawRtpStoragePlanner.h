#pragma once
#include "internal/graph/planner/realtime/MediaRawRtpProducerPlan.h"
#include "internal/graph/planner/realtime/MediaDatagramCommitStoragePlan.h"
#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"

namespace media::ffmpeg::graph {
struct MediaRawRtpStoragePlan final {
    MediaDatagramProtocolEndpointRole role;
    std::size_t maximumMediaDatagrams;
    std::size_t maximumDatagramBytes;
    std::size_t collectionBytes;
    std::size_t wirePreparationBytes;
    std::size_t reservationBytes;
    std::size_t producerSlotBytes;
    std::size_t activeTransactionBytes;
    std::size_t senderHandoffBytes;
    friend bool operator==(const MediaRawRtpStoragePlan&, const MediaRawRtpStoragePlan&) = default;
};

class MediaRawRtpStoragePlanner final {
public:
    static ::media::Result<MediaRawRtpStoragePlan> plan(
        const MediaRawRtpProducerPlan& producer,
        const MediaDatagramCommitStoragePlan& commit,
        const MediaDatagramBatchPlan& batch);
    // Requires one producer slot per stream, one serial global transaction,
    // and one sender submission call. Graph validation enforces no fanout.
    static ::media::Result<std::size_t> ownedStorageUpperBound(
        std::span<const MediaRawRtpStoragePlan> producers);
};
} // namespace media::ffmpeg::graph

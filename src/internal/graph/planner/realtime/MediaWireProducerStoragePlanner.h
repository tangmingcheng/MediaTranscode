#pragma once
#include "internal/graph/planner/realtime/MediaDatagramCommitStoragePlan.h"
#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include <optional>

namespace media::ffmpeg::graph {
// Allocation facts supplied by the protocol planner, never caller parameters.
struct MediaWireProducerStorageFacts final {
    std::size_t maximumEntries;
    std::size_t maximumPayloadBytes;
    std::size_t workspaceBytes;
    std::size_t collectionBytes;
    std::optional<std::size_t> terminalPayloadBytes;
};
struct MediaWireProducerStoragePlan final {
    std::size_t wirePreparationBytes;
    std::size_t reservationBytes;
    std::size_t producerSlotBytes;
    std::size_t activeTransactionBytes;
    std::size_t senderHandoffBytes;
    friend bool operator==(const MediaWireProducerStoragePlan&, const MediaWireProducerStoragePlan&) = default;
};
class MediaWireProducerStoragePlanner final {
public:
    // Requires serial global admission, one pending preparation per producer,
    // a unique sender and no wire fanout. Protocol callers validate MTU facts.
    static ::media::Result<MediaWireProducerStoragePlan> plan(
        const MediaWireProducerStorageFacts& facts,
        const MediaDatagramCommitStoragePlan& commit,
        const MediaDatagramBatchPlan& batch);
};
} // namespace media::ffmpeg::graph

#pragma once
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

namespace media::ffmpeg::graph {
// Synchronous preparation workspace. All backing is split from the complete
// transaction admission; this owner never requests additional budget.
struct MediaMpegTsUdpWirePreparationWorkspace final {
    MediaFixedCapacityStorage<MediaWireGlobalSequenceReservationEntry> globalEntries;
    MediaFixedCapacityStorage<MediaWireDatagramBatchInput> outputInputs;

    static ::media::Result<std::size_t> storageFootprint(
        std::size_t entries);
    static ::media::Result<MediaMpegTsUdpWirePreparationWorkspace> prepare(
        const std::shared_ptr<MediaNetworkMemoryResource>& resource,
        MediaNetworkStorageLease& lease, std::size_t entries);
};
} // namespace media::ffmpeg::graph

#pragma once
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/runtime/storage/MediaOwnedMemoryAllocation.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"

namespace media::ffmpeg::graph {
// Unpublished, single-owner storage for one protocol transaction. All blocks
// share one admission; no later block may independently return WouldBlock.
struct MediaWireCommitPreparation final {
    MediaOwnedMemoryAllocation control;
    std::optional<MediaOwnedMemoryAllocation> actions;
    std::optional<MediaOwnedMemoryAllocation> protocolPrefix;
    MediaWireGlobalSequencePreparedStorage global;
    MediaWireDatagramBatchPartitionBuilder::Prepared output;

    static ::media::Result<std::size_t> storageFootprint(
        const MediaDatagramCommitStoragePlan& plan, std::size_t entries);
    static ::media::Result<MediaNetworkStorageLease> admit(
        const std::shared_ptr<const MediaWireGlobalSequenceState>& identity,
        std::size_t entries, std::size_t outputBytes, std::size_t workspaceBytes);
    static ::media::Result<MediaWireCommitPreparation> fromReservation(
        std::shared_ptr<const MediaWireGlobalSequenceState> identity,
        std::span<const MediaWireDatagramBatchInput> inputs,
        const MediaDatagramBatchPlan& batchPlan, MediaNetworkStorageLease lease);
    static ::media::Result<MediaWireCommitPreparation> prepare(
        std::shared_ptr<const MediaWireGlobalSequenceState> identity,
        std::span<const MediaWireDatagramBatchInput> inputs,
        const MediaDatagramBatchPlan& batchPlan);
};
} // namespace media::ffmpeg::graph

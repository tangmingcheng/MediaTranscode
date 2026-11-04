#pragma once
#include "internal/graph/model/MediaOutputReservationStoragePlan.h"

#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/queue/MediaBlockingQueue.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace media::ffmpeg::graph {

struct MediaOutputCapacityReservationRecord;

class MediaOutputCapacityReservationHandle final {
public:
    MediaOutputCapacityReservationHandle() = default;
    bool valid() const noexcept;
    std::uint64_t identity() const noexcept;

private:
    friend class MediaReservedOutputTransaction;
    explicit MediaOutputCapacityReservationHandle(
        std::shared_ptr<MediaOutputCapacityReservationRecord> record);

    std::shared_ptr<MediaOutputCapacityReservationRecord> m_record;
};

class MediaReservedOutputTransaction final {
public:
    using ReserveResult =
        ::media::Result<std::optional<MediaReservedOutputTransaction>>;
    using Authorization = std::function<::media::Status()>;

    static ReserveResult reserve(
        const char* owner,
        std::span<const MediaAtomicOutputBatch> batches, const MediaOutputReservationStoragePlan& storage);
    static ::media::Status authorize(
        std::span<const MediaOutputCapacityReservationHandle> handles,
        const Authorization& authorization, const MediaOutputReservationStoragePlan& storage);

    MediaReservedOutputTransaction(
        MediaReservedOutputTransaction&& other) noexcept;
    MediaReservedOutputTransaction& operator=(
        MediaReservedOutputTransaction&& other) noexcept;
    MediaReservedOutputTransaction(
        const MediaReservedOutputTransaction&) = delete;
    MediaReservedOutputTransaction& operator=(
        const MediaReservedOutputTransaction&) = delete;
    ~MediaReservedOutputTransaction();

    MediaOutputCapacityReservationHandle handle() const;
    ::media::Status replacePendingBatches(
        std::span<const MediaAtomicOutputBatch> batches);
    ::media::Status commit(
        const Authorization& finalAuthorization = {});
    ::media::Status cancel() noexcept;

private:
    friend class MediaOutputReservationStoragePlanner;
    static ::media::Result<MediaOutputReservationLayout> allocationLayout();
    struct OwnedBatch final {
        MediaChannel* channel = nullptr;
        MediaBlockingQueue* queue = nullptr;
        MediaBlockingQueue::PreparedPush prepared;
    };

    MediaReservedOutputTransaction(
        MediaLineageAllocationScope allocation,
        MediaFixedCapacityStorage<char> owner,
        MediaFixedCapacityStorage<OwnedBatch> batches,
        MediaFixedCapacityStorage<MediaChannel*> channels,
        MediaFixedCapacityStorage<std::unique_lock<std::mutex>> channelLocks,
        MediaFixedCapacityStorage<std::unique_lock<std::mutex>> queueLocks,
        std::shared_ptr<MediaOutputCapacityReservationRecord> record) noexcept;

    MediaLineageAllocationScope m_allocation;
    MediaFixedCapacityStorage<char> m_owner;
    MediaFixedCapacityStorage<OwnedBatch> m_batches;
    MediaFixedCapacityStorage<MediaChannel*> m_channels;
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> m_channelLocks;
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> m_queueLocks;
    std::shared_ptr<MediaOutputCapacityReservationRecord> m_record;
};

} // namespace media::ffmpeg::graph

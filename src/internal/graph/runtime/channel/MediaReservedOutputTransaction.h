#pragma once

#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"

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
        std::span<const MediaAtomicOutputBatch> batches);
    static ::media::Status authorize(
        std::span<const MediaOutputCapacityReservationHandle> handles,
        const Authorization& authorization);

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
    struct OwnedBatch final {
        MediaChannel* channel = nullptr;
        std::vector<MediaBufferRef> buffers;
    };

    MediaReservedOutputTransaction(
        std::string owner,
        std::vector<OwnedBatch> batches,
        std::shared_ptr<MediaOutputCapacityReservationRecord> record);
    static std::vector<std::unique_lock<std::mutex>> lockChannels(
        const std::vector<MediaChannel*>& channels);

    std::string m_owner;
    std::vector<OwnedBatch> m_batches;
    std::shared_ptr<MediaOutputCapacityReservationRecord> m_record;
};

} // namespace media::ffmpeg::graph

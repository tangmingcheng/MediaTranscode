#pragma once

#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressBatch.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtpIngressWritableSlot final {
    std::size_t slotIndex;
    std::size_t byteOffset;
    std::span<std::byte> bytes;
};

class MediaRtpIngressStorageState final {
public:
    ~MediaRtpIngressStorageState();

private:
    friend class MediaRtpIngressBatch;
    friend class MediaRtpIngressStorage;

    enum class SlotOwnership : std::uint8_t {
        Free,
        ReceiveOwned,
        BatchOwned
    };

    void releaseBatch() noexcept;

    std::byte* arena = nullptr;
    std::size_t byteCapacity = 0;
    std::size_t maximumDatagramBytes = 0;
    std::size_t descriptorCapacity = 0;
    std::size_t alignmentBytes = 0;
    std::vector<MediaRtpIngressBatchEntry> entries;
    std::vector<std::size_t> entrySlotIndices;
    std::vector<SlotOwnership> slotOwnership;
    std::size_t committedEntries = 0;
    std::size_t nextFreeSearch = 0;
    std::int64_t lastCommittedObservationNanoseconds = 0;
    bool leased = false;
};

class MediaRtpIngressStorage final {
public:
    MediaRtpIngressStorage() = delete;
    ~MediaRtpIngressStorage() = default;

    MediaRtpIngressStorage(const MediaRtpIngressStorage&) = delete;
    MediaRtpIngressStorage& operator=(const MediaRtpIngressStorage&) = delete;
    MediaRtpIngressStorage(MediaRtpIngressStorage&& other) noexcept = default;
    MediaRtpIngressStorage& operator=(MediaRtpIngressStorage&& other) noexcept = default;

    static ::media::Result<MediaRtpIngressStorage> create(
        std::size_t byteCapacity,
        std::size_t maximumDatagramBytes,
        std::size_t descriptorCapacity,
        std::size_t alignmentBytes);

    std::byte* data() noexcept;
    std::size_t byteCapacity() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t descriptorCapacity() const noexcept;
    std::size_t committedEntries() const noexcept;
    ::media::Result<MediaRtpIngressWritableSlot> acquireReceiveSlot();
    ::media::Status releaseReceiveSlot(std::size_t slotIndex);
    ::media::Status commitReceivedSlot(
        std::size_t slotIndex,
        MediaRtpUdpChannel channel,
        std::size_t byteCount,
        std::int64_t observedAtNanoseconds);
    ::media::Result<MediaRtpIngressBatch> seal(std::size_t entryCount);
    ::media::Status reset();

private:
    friend class MediaRtpIngressBatch;
    explicit MediaRtpIngressStorage(
        std::shared_ptr<MediaRtpIngressStorageState> state) noexcept;

    std::shared_ptr<MediaRtpIngressStorageState> m_state;
};

} // namespace media::ffmpeg::graph

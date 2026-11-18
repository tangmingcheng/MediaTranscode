#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpChannel.h"
#include "internal/graph/runtime/buffer/MediaPreparedRtpDatagramStorage.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace media::ffmpeg::graph {

class MediaRawRtpPreparedByteBudget;
class MediaRawRtpPreparedInputBuffer;

struct MediaRawRtpProbeDatagram final {
    MediaRtpUdpChannel channel;
    std::span<const std::uint8_t> bytes;
    std::int64_t observedAtNs;
};

// Pins immutable entries through a frozen reference table, independently of
// queue positions. An active lease prevents replay sealing.
// Views remain valid only while this lease (or its moved-to owner) is alive.
class MediaRawRtpProbeLease final {
public:
    ~MediaRawRtpProbeLease();
    MediaRawRtpProbeLease(const MediaRawRtpProbeLease&) = delete;
    MediaRawRtpProbeLease& operator=(const MediaRawRtpProbeLease&) = delete;
    MediaRawRtpProbeLease(MediaRawRtpProbeLease&& other) noexcept;
    MediaRawRtpProbeLease& operator=(MediaRawRtpProbeLease&& other) noexcept;

    std::size_t datagramCount() const noexcept;
    std::uint64_t ingressFrontier() const noexcept;
    ::media::Result<MediaRawRtpProbeDatagram> datagram(std::size_t index) const;

private:
    friend class MediaRawRtpPreparedInputBuffer;
    MediaRawRtpProbeLease(
        std::shared_ptr<MediaRawRtpPreparedByteBudget> budget,
        MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner> entries,
        std::uint64_t frontier,
        std::shared_ptr<const MediaRawRtpPreparedInputBuffer> owner) noexcept;
    static ::media::Result<MediaRawRtpProbeLease> capture(
        const MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner>& entries,
        std::uint64_t frontier,
        const std::shared_ptr<MediaRawRtpPreparedByteBudget>& budget,
        std::shared_ptr<const MediaRawRtpPreparedInputBuffer> owner);
    void release() noexcept;

    std::shared_ptr<MediaRawRtpPreparedByteBudget> m_budget;
    std::shared_ptr<const MediaRawRtpPreparedInputBuffer> m_owner;
    MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner> m_entries;
    std::uint64_t m_frontier = 0;
};

} // namespace media::ffmpeg::graph

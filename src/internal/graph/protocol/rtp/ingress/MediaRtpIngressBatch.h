#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpChannel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace media::ffmpeg::graph {

struct MediaRtpIngressBatchEntry final {
    MediaRtpUdpChannel channel;
    std::span<const std::byte> bytes;
    std::int64_t observedAtNanoseconds;
};

struct MediaRtpIngressStorageState;

class MediaRtpIngressBatch final {
public:
    MediaRtpIngressBatch() = delete;
    ~MediaRtpIngressBatch();

    MediaRtpIngressBatch(const MediaRtpIngressBatch&) = delete;
    MediaRtpIngressBatch& operator=(const MediaRtpIngressBatch&) = delete;
    MediaRtpIngressBatch(MediaRtpIngressBatch&& other) noexcept;
    MediaRtpIngressBatch& operator=(MediaRtpIngressBatch&& other) noexcept;

    std::span<const MediaRtpIngressBatchEntry> entries() const noexcept;

private:
    // A batch lease and its storage are owned by one receiver worker. The lease
    // may be moved, but it must not be accessed or destroyed concurrently.
    friend class MediaRtpIngressStorage;
    MediaRtpIngressBatch(
        std::shared_ptr<MediaRtpIngressStorageState> storage,
        std::span<const MediaRtpIngressBatchEntry> entries) noexcept;
    void release() noexcept;

    std::shared_ptr<MediaRtpIngressStorageState> m_storage;
    std::span<const MediaRtpIngressBatchEntry> m_entries;
};

} // namespace media::ffmpeg::graph

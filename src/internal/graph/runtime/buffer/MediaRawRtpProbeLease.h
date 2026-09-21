#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpChannel.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>

namespace media::ffmpeg::graph {

class MediaRawRtpPreparedByteBudget;
class MediaRawRtpPreparedInputBuffer;
struct MediaPreparedRawRtpDatagram;

struct MediaRawRtpProbeDatagram final {
    MediaRtpUdpChannel channel;
    std::span<const std::uint8_t> bytes;
    std::int64_t observedAtNs;
};

// Owns an immutable preflight snapshot, without consuming or activating replay.
// Views remain valid only while this lease (or its moved-to owner) is alive.
class MediaRawRtpProbeLease final {
public:
    ~MediaRawRtpProbeLease();
    MediaRawRtpProbeLease(const MediaRawRtpProbeLease&) = delete;
    MediaRawRtpProbeLease& operator=(const MediaRawRtpProbeLease&) = delete;
    MediaRawRtpProbeLease(MediaRawRtpProbeLease&& other) noexcept;
    MediaRawRtpProbeLease& operator=(MediaRawRtpProbeLease&& other) noexcept;

    std::span<const MediaRawRtpProbeDatagram> datagrams() const noexcept;

private:
    friend class MediaRawRtpPreparedInputBuffer;
    MediaRawRtpProbeLease(
        std::shared_ptr<MediaRawRtpPreparedByteBudget> budget,
        std::size_t reservedBytes) noexcept;
    static ::media::Result<MediaRawRtpProbeLease> capture(
        const std::deque<MediaPreparedRawRtpDatagram>& datagrams,
        const std::shared_ptr<MediaRawRtpPreparedByteBudget>& budget);
    void release() noexcept;

    std::shared_ptr<MediaRawRtpPreparedByteBudget> m_budget;
    std::unique_ptr<std::uint8_t[]> m_payload;
    std::unique_ptr<MediaRawRtpProbeDatagram[]> m_datagrams;
    std::size_t m_count = 0;
    std::size_t m_reservedBytes = 0;
};

} // namespace media::ffmpeg::graph

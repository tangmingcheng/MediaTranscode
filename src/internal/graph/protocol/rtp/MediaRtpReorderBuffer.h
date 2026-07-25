#pragma once

#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "media_transcode/Result.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRtpDiscontinuityReason {
    SequenceGap,
    SsrcChanged,
    PayloadTypeChanged
};

struct MediaRtpDiscontinuity final {
    MediaRtpDiscontinuityReason reason;
    uint16_t firstMissingSequence;
    uint16_t resumedSequence;
};

struct MediaRtpReorderConfig final {
    std::size_t windowPackets;
    std::chrono::milliseconds maximumDelay;
    uint8_t payloadType;
};

struct MediaRtpReorderResult final {
    std::vector<MediaRtpPacket> packets;
    std::vector<MediaRtpDiscontinuity> discontinuities;
    bool duplicate = false;
};

class MediaRtpReorderBuffer final {
public:
    explicit MediaRtpReorderBuffer(MediaRtpReorderConfig config);
    ::media::Result<MediaRtpReorderResult> push(MediaRtpPacket packet,
                                                std::chrono::steady_clock::time_point receivedAt);
    std::optional<std::chrono::steady_clock::time_point> nextDeadline() const noexcept;
    ::media::Result<MediaRtpReorderResult> expire(std::chrono::steady_clock::time_point now);
    void reset() noexcept;

private:
    struct Entry final {
        MediaRtpPacket packet;
        std::chrono::steady_clock::time_point receivedAt;
    };
    void drainContiguous(MediaRtpReorderResult& result);
    std::map<uint16_t, Entry>::const_iterator nearestPending() const noexcept;

    MediaRtpReorderConfig m_config;
    std::optional<uint16_t> m_expected;
    std::optional<uint32_t> m_ssrc;
    std::map<uint16_t, Entry> m_pending;
};

} // namespace media::ffmpeg::graph

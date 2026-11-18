#pragma once

#include "internal/graph/protocol/rtp/MediaRtpTimestamp.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <span>
#include <cstddef>

namespace media::ffmpeg::graph {

class MediaRtpDatagramRewriteIdentity final {
public:
    static ::media::Result<MediaRtpDatagramRewriteIdentity> create(
        int payloadType,
        std::uint32_t ssrc) noexcept;

    int payloadType() const noexcept { return m_payloadType; }
    std::uint32_t ssrc() const noexcept { return m_ssrc; }

private:
    MediaRtpDatagramRewriteIdentity(int payloadType,
                                    std::uint32_t ssrc) noexcept;

    int m_payloadType;
    std::uint32_t m_ssrc;
};

// Borrowed input must remain alive and immutable until synchronous writeInto.
class MediaRtpDatagramRewritePlan final {
public:
    std::size_t bytes() const noexcept { return m_datagram.size(); }
    std::size_t payloadOctets() const noexcept { return m_payloadOctets; }
private:
    friend class MediaRtpDatagramRewriter;
    MediaRtpDatagramRewritePlan(std::span<const std::uint8_t> datagram,
        MediaRtpDatagramRewriteIdentity identity, std::size_t payloadOctets) noexcept
        : m_datagram(datagram), m_identity(identity), m_payloadOctets(payloadOctets) {}
    std::span<const std::uint8_t> m_datagram;
    MediaRtpDatagramRewriteIdentity m_identity;
    std::size_t m_payloadOctets;
};

class MediaRtpDatagramRewriter final {
public:
    static ::media::Result<MediaRtpDatagramRewritePlan> prepare(
        std::span<const std::uint8_t> datagram,
        const MediaRtpDatagramRewriteIdentity& identity);
    static ::media::Status writeInto(const MediaRtpDatagramRewritePlan& plan,
        MediaRtpTimestamp timestamp, std::span<std::uint8_t> output);
};
} // namespace media::ffmpeg::graph

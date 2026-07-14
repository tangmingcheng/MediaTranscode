#pragma once

#include "internal/graph/protocol/rtp/MediaRtpTimestamp.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <span>
#include <vector>

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

class MediaRtpDatagramRewriteParameters final {
public:
    MediaRtpDatagramRewriteParameters(
        MediaRtpDatagramRewriteIdentity identity,
        MediaRtpTimestamp timestamp) noexcept;

    const MediaRtpDatagramRewriteIdentity& identity() const noexcept
    {
        return m_identity;
    }
    MediaRtpTimestamp timestamp() const noexcept { return m_timestamp; }

private:
    MediaRtpDatagramRewriteIdentity m_identity;
    MediaRtpTimestamp m_timestamp;
};

class MediaRtpDatagramRewriter final {
public:
    static ::media::Status rewrite(
        std::span<const std::uint8_t> datagram,
        const MediaRtpDatagramRewriteParameters& parameters,
        std::vector<std::uint8_t>& output);
};

} // namespace media::ffmpeg::graph

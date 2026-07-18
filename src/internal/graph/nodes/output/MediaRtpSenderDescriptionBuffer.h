#pragma once

#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtpSenderDescriptionBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaScheduledStream stream,
        std::uint64_t generation,
        MediaSdpSessionIdentity session,
        MediaRtpSdpMediaDescription media);

    MediaBufferType type() const noexcept override;
    MediaScheduledStream stream() const noexcept { return m_stream; }
    std::uint64_t generation() const noexcept { return m_generation; }
    const MediaSdpSessionIdentity& session() const noexcept { return m_session; }
    const MediaRtpSdpMediaDescription& media() const noexcept { return m_media; }

private:
    MediaRtpSenderDescriptionBuffer(
        MediaScheduledStream stream,
        std::uint64_t generation,
        MediaSdpSessionIdentity session,
        MediaRtpSdpMediaDescription media);

    MediaScheduledStream m_stream;
    std::uint64_t m_generation;
    MediaSdpSessionIdentity m_session;
    MediaRtpSdpMediaDescription m_media;
};

} // namespace media::ffmpeg::graph

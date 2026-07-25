#pragma once

#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace media::ffmpeg::graph {

class MediaTsPesHeader final {
public:
    MediaScheduledStream stream() const noexcept { return m_stream; }
    std::size_t framedPayloadBytes() const noexcept
    {
        return m_framedPayloadBytes;
    }
    std::span<const std::uint8_t> bytes() const noexcept
    {
        return std::span<const std::uint8_t>(m_bytes.data(), m_size);
    }

private:
    friend class MediaTsPesSerializer;
    MediaTsPesHeader(MediaScheduledStream stream,
                     std::array<std::uint8_t, 19> bytes,
                     std::size_t size,
                     std::size_t framedPayloadBytes) noexcept
        : m_stream(stream),
          m_bytes(std::move(bytes)),
          m_size(size),
          m_framedPayloadBytes(framedPayloadBytes) {}

    MediaScheduledStream m_stream;
    std::array<std::uint8_t, 19> m_bytes{};
    std::size_t m_size = 0;
    std::size_t m_framedPayloadBytes = 0;
};

class MediaTsPesSerializer final {
public:
    static ::media::Result<MediaTsPesHeader> header(
        MediaScheduledStream stream,
        const MediaTsPacketClock& clock,
        std::size_t framedPayloadBytes);
};

} // namespace media::ffmpeg::graph

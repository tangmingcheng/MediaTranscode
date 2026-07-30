#pragma once

#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaMpegTsRtpPacketizerConfig final {
    int payloadType;
    int clockRate;
    std::uint32_t ssrc;
    std::uint32_t baseTimestamp;
    std::uint16_t initialSequenceNumber;
    std::uint8_t maximumTsPackets;
    std::size_t maximumDatagramBytes;
    MediaRunningTime masterOrigin;
};

class MediaMpegTsRtpPacket final {
public:
    const std::vector<std::uint8_t>& datagram() const noexcept
    {
        return m_datagram;
    }
    std::size_t payloadOctets() const noexcept { return m_payloadOctets; }
    std::uint16_t sequenceNumber() const noexcept { return m_sequenceNumber; }
    MediaRtpTimestamp timestamp() const noexcept { return m_timestamp; }

private:
    friend class MediaMpegTsRtpPacketizer;

    MediaMpegTsRtpPacket(
        std::vector<std::uint8_t> datagram,
        std::size_t payloadOctets,
        std::uint16_t sequenceNumber,
        MediaRtpTimestamp timestamp) noexcept;

    std::vector<std::uint8_t> m_datagram;
    std::size_t m_payloadOctets;
    std::uint16_t m_sequenceNumber;
    MediaRtpTimestamp m_timestamp;
};

class MediaMpegTsRtpPacketizer final {
public:
    static ::media::Result<MediaMpegTsRtpPacketizer> create(
        MediaMpegTsRtpPacketizerConfig config);

    ::media::Result<MediaMpegTsRtpPacket> packetize(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime emitOnMaster);

    const MediaRtpOutputClockMapper& clockMapper() const noexcept
    {
        return m_clockMapper;
    }

private:
    MediaMpegTsRtpPacketizer(
        MediaMpegTsRtpPacketizerConfig config,
        MediaRtpOutputClockMapper clockMapper) noexcept;

    MediaMpegTsRtpPacketizerConfig m_config;
    MediaRtpOutputClockMapper m_clockMapper;
    std::optional<MediaRtpTimestamp> m_lastTimestamp;
    std::uint16_t m_nextSequenceNumber;
};

} // namespace media::ffmpeg::graph

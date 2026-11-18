#pragma once

#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>


namespace media::ffmpeg::graph {

struct MediaMpegTsRtpPacketizerConfig final {
    int payloadType;
    int clockRate;
    std::uint32_t ssrc;
    std::uint32_t baseTimestamp;
    std::uint16_t maximumTsPackets;
    std::size_t maximumDatagramBytes;
    MediaRunningTime masterOrigin;
};

// Borrows TS bytes until synchronous serialization; no mutable protocol state.
class MediaMpegTsRtpPacketPlan final {
public:
    std::size_t bytes() const noexcept { return m_bytes; }
    std::size_t payloadOctets() const noexcept { return m_payload.size(); }
    std::uint16_t sequenceNumber() const noexcept { return m_sequence; }
    MediaRtpTimestamp timestamp() const noexcept { return m_timestamp; }
private:
    friend class MediaMpegTsRtpPacketizer;
    friend class MediaRtpWirePacketComposer;
    MediaMpegTsRtpPacketPlan(std::span<const std::uint8_t> payload,
        std::size_t bytes, std::uint16_t sequence, MediaRtpTimestamp timestamp,
        std::uint32_t ssrc, int payloadType) noexcept
        : m_payload(payload), m_bytes(bytes), m_sequence(sequence),
          m_timestamp(timestamp), m_ssrc(ssrc), m_payloadType(payloadType) {}
    std::span<const std::uint8_t> m_payload;
    std::size_t m_bytes;
    std::uint16_t m_sequence;
    MediaRtpTimestamp m_timestamp;
    std::uint32_t m_ssrc;
    int m_payloadType;
};
class MediaMpegTsRtpPacketizer final {
public:
    static ::media::Result<MediaMpegTsRtpPacketizer> create(
        MediaMpegTsRtpPacketizerConfig config);

    ::media::Result<MediaMpegTsRtpPacketPlan> prepare(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime emitOnMaster,
        std::uint16_t sequenceNumber) const;

    static ::media::Status writeInto(const MediaMpegTsRtpPacketPlan& plan,
        std::span<std::uint8_t> output);

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
};

} // namespace media::ffmpeg::graph

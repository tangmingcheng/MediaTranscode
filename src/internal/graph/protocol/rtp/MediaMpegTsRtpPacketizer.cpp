#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"

#include <algorithm>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t RtpHeaderBytes = 12;
constexpr std::size_t TsPacketBytes = 188;
constexpr int Mp2tStaticPayloadType = 33;
constexpr int Mp2tClockRate = 90'000;

void writeU16(std::vector<std::uint8_t>& datagram,
              std::size_t offset,
              std::uint16_t value) noexcept
{
    datagram[offset] = static_cast<std::uint8_t>(value >> 8);
    datagram[offset + 1] = static_cast<std::uint8_t>(value);
}

void writeU32(std::vector<std::uint8_t>& datagram,
              std::size_t offset,
              std::uint32_t value) noexcept
{
    datagram[offset] = static_cast<std::uint8_t>(value >> 24);
    datagram[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    datagram[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    datagram[offset + 3] = static_cast<std::uint8_t>(value);
}

} // namespace

MediaMpegTsRtpPacket::MediaMpegTsRtpPacket(
    std::vector<std::uint8_t> datagram,
    std::size_t payloadOctets,
    std::uint16_t sequenceNumber,
    MediaRtpTimestamp timestamp) noexcept
    : m_datagram(std::move(datagram)),
      m_payloadOctets(payloadOctets),
      m_sequenceNumber(sequenceNumber),
      m_timestamp(timestamp)
{
}

::media::Result<MediaMpegTsRtpPacketizer>
MediaMpegTsRtpPacketizer::create(MediaMpegTsRtpPacketizerConfig config)
{
    if (config.payloadType != Mp2tStaticPayloadType ||
        config.clockRate != Mp2tClockRate || config.ssrc == 0 ||
        config.maximumTsPackets < 1 || config.maximumTsPackets > 7 ||
        config.maximumDatagramBytes < RtpHeaderBytes + TsPacketBytes ||
        static_cast<std::size_t>(config.maximumTsPackets) >
            (config.maximumDatagramBytes - RtpHeaderBytes) / TsPacketBytes) {
        return ::media::Result<MediaMpegTsRtpPacketizer>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T RTP packetizer requires complete planner-owned PT 33, 90 kHz, SSRC, sequence, and datagram facts"));
    }
    auto mapper = MediaRtpOutputClockMapper::create(
        config.clockRate, config.baseTimestamp, config.masterOrigin);
    if (!mapper) {
        return ::media::Result<MediaMpegTsRtpPacketizer>::failure(
            mapper.error());
    }
    return ::media::Result<MediaMpegTsRtpPacketizer>::success(
        MediaMpegTsRtpPacketizer(config, mapper.value()));
}

MediaMpegTsRtpPacketizer::MediaMpegTsRtpPacketizer(
    MediaMpegTsRtpPacketizerConfig config,
    MediaRtpOutputClockMapper clockMapper) noexcept
    : m_config(std::move(config)), m_clockMapper(clockMapper)
{
}

::media::Result<MediaMpegTsRtpPacket>
MediaMpegTsRtpPacketizer::packetize(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime emitOnMaster,
    std::uint16_t sequenceNumber) const
{
    const std::size_t payloadBytes = completeTsPackets.size();
    const std::size_t packetCount = payloadBytes / TsPacketBytes;
    if (payloadBytes == 0 || payloadBytes % TsPacketBytes != 0 ||
        packetCount > m_config.maximumTsPackets ||
        payloadBytes > m_config.maximumDatagramBytes - RtpHeaderBytes) {
        return ::media::Result<MediaMpegTsRtpPacket>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T RTP payload must contain one planned batch of complete TS packets"));
    }
    auto timestamp = m_clockMapper.map(emitOnMaster);
    if (!timestamp) {
        return ::media::Result<MediaMpegTsRtpPacket>::failure(
            timestamp.error());
    }
    try {
        std::vector<std::uint8_t> datagram(
            RtpHeaderBytes + payloadBytes);
        datagram[0] = 0x80;
        datagram[1] =
            static_cast<std::uint8_t>(m_config.payloadType);
        writeU16(datagram, 2, sequenceNumber);
        writeU32(datagram, 4, timestamp.value().wire());
        writeU32(datagram, 8, m_config.ssrc);
        std::copy(
            completeTsPackets.begin(), completeTsPackets.end(),
            datagram.begin() +
                static_cast<std::ptrdiff_t>(RtpHeaderBytes));

        return ::media::Result<MediaMpegTsRtpPacket>::success(
            MediaMpegTsRtpPacket(
                std::move(datagram), payloadBytes, sequenceNumber,
                timestamp.value()));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaMpegTsRtpPacket>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaMpegTsRtpPacketizer"));
    }
}

} // namespace media::ffmpeg::graph

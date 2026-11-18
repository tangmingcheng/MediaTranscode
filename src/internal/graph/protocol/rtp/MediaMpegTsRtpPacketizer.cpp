#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t RtpHeaderBytes = 12;
constexpr std::size_t TsPacketBytes = 188;
constexpr int Mp2tStaticPayloadType = 33;
constexpr int Mp2tClockRate = 90'000;

void writeU16(std::span<std::uint8_t> datagram,
              std::size_t offset,
              std::uint16_t value) noexcept
{
    datagram[offset] = static_cast<std::uint8_t>(value >> 8);
    datagram[offset + 1] = static_cast<std::uint8_t>(value);
}

void writeU32(std::span<std::uint8_t> datagram,
              std::size_t offset,
              std::uint32_t value) noexcept
{
    datagram[offset] = static_cast<std::uint8_t>(value >> 24);
    datagram[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    datagram[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    datagram[offset + 3] = static_cast<std::uint8_t>(value);
}

} // namespace

::media::Result<MediaMpegTsRtpPacketizer>
MediaMpegTsRtpPacketizer::create(MediaMpegTsRtpPacketizerConfig config)
{
    if (config.payloadType != Mp2tStaticPayloadType ||
        config.clockRate != Mp2tClockRate || config.ssrc == 0 ||
        config.maximumTsPackets < 1 ||
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

::media::Result<MediaMpegTsRtpPacketPlan>
MediaMpegTsRtpPacketizer::prepare(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime emitOnMaster,
    std::uint16_t sequenceNumber) const
{
    const std::size_t payloadBytes = completeTsPackets.size();
    const std::size_t packetCount = payloadBytes / TsPacketBytes;
    if (payloadBytes == 0 || payloadBytes % TsPacketBytes != 0 ||
        packetCount > m_config.maximumTsPackets ||
        payloadBytes > m_config.maximumDatagramBytes - RtpHeaderBytes) {
        return ::media::Result<MediaMpegTsRtpPacketPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T RTP payload must contain one planned batch of complete TS packets"));
    }
    auto timestamp = m_clockMapper.map(emitOnMaster);
    if (!timestamp) {
        return ::media::Result<MediaMpegTsRtpPacketPlan>::failure(
            timestamp.error());
    }
    return ::media::Result<MediaMpegTsRtpPacketPlan>::success(
        MediaMpegTsRtpPacketPlan(completeTsPackets, RtpHeaderBytes + payloadBytes,
            sequenceNumber, timestamp.value(), m_config.ssrc, m_config.payloadType));
}

::media::Status MediaMpegTsRtpPacketizer::writeInto(
    const MediaMpegTsRtpPacketPlan& plan, std::span<std::uint8_t> output)
{
    if (output.size() != plan.bytes()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("MP2T output differs from its exact prepared size"));
    // Copy first so overlapping source storage cannot be overwritten by headers.
    std::memmove(output.data() + RtpHeaderBytes, plan.m_payload.data(), plan.m_payload.size());
    output[0] = 0x80;
    output[1] = static_cast<std::uint8_t>(plan.m_payloadType);
    writeU16(output, 2, plan.m_sequence);
    writeU32(output, 4, plan.m_timestamp.wire());
    writeU32(output, 8, plan.m_ssrc);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

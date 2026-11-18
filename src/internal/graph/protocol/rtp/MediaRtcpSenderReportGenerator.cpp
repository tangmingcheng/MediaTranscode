#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"
#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
constexpr auto SenderReportBytes = MediaRtcpWireGeometry::senderReportPayloadBytes();
constexpr auto SdesPrefixBytes = MediaRtcpWireGeometry::sdesCnamePrefixBytes();
void writeU16(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value) noexcept
{
    bytes[offset] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 1] = static_cast<std::uint8_t>(value);
}
void writeU32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::uint8_t>(value >> 24);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}
} // namespace
MediaRtcpSenderReportTimestamp::MediaRtcpSenderReportTimestamp(
    MediaRunningTime masterInstant,
    MediaNtpTimestamp ntp,
    MediaRtpTimestamp rtp) noexcept
    : m_masterInstant(masterInstant),
      m_ntp(ntp),
      m_rtp(rtp)
{
}

MediaRtcpSenderReportParameters::MediaRtcpSenderReportParameters(
    std::uint32_t ssrcValue,
    std::string_view cnameValue,
    MediaRtcpSenderReportTimestamp timestampValue,
    std::uint64_t senderPacketCountValue,
    std::uint64_t senderOctetCountValue)
    : ssrc(ssrcValue),
      cname(std::move(cnameValue)),
      timestamp(std::move(timestampValue)),
      senderPacketCount(senderPacketCountValue),
      senderOctetCount(senderOctetCountValue)
{
}

::media::Result<MediaRtcpSenderReportTimestamp>
MediaRtcpSenderReportGenerator::mapTimestamp(
    MediaRunningTime masterInstant,
    const MediaSharedNtpEpoch& ntpEpoch,
    const MediaRtpOutputClockMapper& rtpMapper) noexcept
{
    auto ntp = ntpEpoch.map(masterInstant);
    if (!ntp) {
        return ::media::Result<MediaRtcpSenderReportTimestamp>::failure(
            ntp.error());
    }
    auto rtp = rtpMapper.map(masterInstant);
    if (!rtp) {
        return ::media::Result<MediaRtcpSenderReportTimestamp>::failure(
            rtp.error());
    }
    return ::media::Result<MediaRtcpSenderReportTimestamp>::success(
        MediaRtcpSenderReportTimestamp(
            masterInstant, std::move(ntp).value(), std::move(rtp).value()));
}

::media::Result<MediaRtcpSenderReportPlan> MediaRtcpSenderReportGenerator::prepare(
    const MediaRtcpSenderReportParameters& parameters, bool includeBye)
{
    using Result = ::media::Result<MediaRtcpSenderReportPlan>;
    if (!parameters.ssrc) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTCP sender report SSRC must be non-zero"));
    auto valid = MediaRtcpSdesTextValidator::validateCname(parameters.cname);
    if (!valid) return Result::failure(valid.error());
    auto sdesBytes = MediaRtcpWireGeometry::sdesPayloadBytes(parameters.cname.size());
    if (!sdesBytes) return Result::failure(sdesBytes.error());
    // The validated one-byte CNAME length bounds this complete packet.
    const auto totalBytes = SenderReportBytes + sdesBytes.value()
        + (includeBye ? MediaRtcpWireGeometry::byePayloadBytes() : 0);
    return Result::success(MediaRtcpSenderReportPlan(parameters,
        static_cast<std::size_t>(sdesBytes.value()),
        static_cast<std::size_t>(totalBytes), includeBye));
}

::media::Status MediaRtcpSenderReportGenerator::writeInto(
    const MediaRtcpSenderReportPlan& plan, std::span<std::uint8_t> output)
{
    if (output.size() != plan.bytes()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("RTCP output differs from its exact prepared size"));
    const auto& parameters = plan.m_parameters;
    const auto cnameOffset = SenderReportBytes + SdesPrefixBytes;
    // Copy borrowed content before writing headers, including overlapping input.
    std::memmove(output.data() + cnameOffset, parameters.cname.data(), parameters.cname.size());
    std::fill(output.begin() + cnameOffset + parameters.cname.size(),
        output.begin() + SenderReportBytes + plan.m_sdesBytes, std::uint8_t{0});
    output[0] = 0x80;
    output[1] = 200;
    writeU16(output, 2, 6);
    writeU32(output, 4, parameters.ssrc);
    const auto ntp = parameters.timestamp.ntp().wire();
    writeU32(output, 8, ntp.seconds);
    writeU32(output, 12, ntp.fraction);
    writeU32(output, 16, parameters.timestamp.rtp().wire());
    writeU32(output, 20, static_cast<std::uint32_t>(parameters.senderPacketCount));
    writeU32(output, 24, static_cast<std::uint32_t>(parameters.senderOctetCount));
    output[SenderReportBytes] = 0x81;
    output[SenderReportBytes + 1] = 202;
    writeU16(output, SenderReportBytes + 2, static_cast<std::uint16_t>(plan.m_sdesBytes / 4 - 1));
    writeU32(output, SenderReportBytes + 4, parameters.ssrc);
    output[SenderReportBytes + 8] = 1;
    output[SenderReportBytes + 9] = static_cast<std::uint8_t>(parameters.cname.size());
    if (plan.m_includeBye) {
        const auto offset = SenderReportBytes + plan.m_sdesBytes;
        output[offset] = 0x81;
        output[offset + 1] = 203;
        writeU16(output, offset + 2, 1);
        writeU32(output, offset + 4, parameters.ssrc);
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

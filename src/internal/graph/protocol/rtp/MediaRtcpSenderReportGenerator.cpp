#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"
#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"

#include <cstddef>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using BytesResult = ::media::Result<std::vector<std::uint8_t>>;

BytesResult invalidReport(const char* message)
{
    return BytesResult::failure(::media::ErrorInfo::invalidArgument(message));
}

void appendU16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

::media::Status validate(const MediaRtcpSenderReportParameters& parameters)
{
    if (parameters.ssrc == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTCP sender report SSRC must be non-zero"));
    }
    if (auto cname = MediaRtcpSdesTextValidator::validateCname(parameters.cname);
        !cname) return cname;
    if (parameters.senderPacketCount >
            std::numeric_limits<std::uint32_t>::max() ||
        parameters.senderOctetCount >
            std::numeric_limits<std::uint32_t>::max()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTCP sender report counters exceed their wire fields"));
    }
    return ::media::Status::success();
}

void appendSenderReport(
    std::vector<std::uint8_t>& bytes,
    const MediaRtcpSenderReportParameters& parameters)
{
    bytes.push_back(0x80);
    bytes.push_back(200);
    appendU16(bytes, 6);
    appendU32(bytes, parameters.ssrc);
    const MediaNtpWireTimestamp ntp = parameters.timestamp.ntp().wire();
    appendU32(bytes, ntp.seconds);
    appendU32(bytes, ntp.fraction);
    appendU32(bytes, parameters.timestamp.rtp().wire());
    appendU32(bytes, static_cast<std::uint32_t>(parameters.senderPacketCount));
    appendU32(bytes, static_cast<std::uint32_t>(parameters.senderOctetCount));
}

void appendSourceDescription(
    std::vector<std::uint8_t>& bytes,
    const MediaRtcpSenderReportParameters& parameters)
{
    const std::size_t packetStart = bytes.size();
    bytes.push_back(0x81);
    bytes.push_back(202);
    appendU16(bytes, 0);
    appendU32(bytes, parameters.ssrc);
    bytes.push_back(1);
    bytes.push_back(static_cast<std::uint8_t>(parameters.cname.size()));
    bytes.insert(bytes.end(), parameters.cname.begin(), parameters.cname.end());
    bytes.push_back(0);
    while ((bytes.size() - packetStart) % 4 != 0) bytes.push_back(0);

    const std::size_t words = (bytes.size() - packetStart) / 4;
    const std::uint16_t length = static_cast<std::uint16_t>(words - 1);
    bytes[packetStart + 2] = static_cast<std::uint8_t>(length >> 8);
    bytes[packetStart + 3] = static_cast<std::uint8_t>(length);
}

void appendBye(std::vector<std::uint8_t>& bytes, std::uint32_t ssrc)
{
    bytes.push_back(0x81);
    bytes.push_back(203);
    appendU16(bytes, 1);
    appendU32(bytes, ssrc);
}

BytesResult serializeCompound(
    const MediaRtcpSenderReportParameters& parameters,
    bool includeBye)
{
    if (auto status = validate(parameters); !status) {
        return BytesResult::failure(status.error());
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(28 + 12 + parameters.cname.size() + (includeBye ? 8 : 0));
    appendSenderReport(bytes, parameters);
    appendSourceDescription(bytes, parameters);
    if (includeBye) appendBye(bytes, parameters.ssrc);
    if (bytes.size() % 4 != 0) {
        return invalidReport("RTCP compound packet alignment is invalid");
    }
    return BytesResult::success(std::move(bytes));
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
    std::string cnameValue,
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

::media::Result<std::vector<std::uint8_t>>
MediaRtcpSenderReportGenerator::serialize(
    const MediaRtcpSenderReportParameters& parameters)
{
    return serializeCompound(parameters, false);
}

::media::Result<std::vector<std::uint8_t>>
MediaRtcpSenderReportGenerator::serializeWithBye(
    const MediaRtcpSenderReportParameters& parameters)
{
    return serializeCompound(parameters, true);
}

} // namespace media::ffmpeg::graph

#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"

#include <algorithm>
#include <cstddef>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t FixedHeaderBytes = 12;
constexpr int MinimumDynamicPayloadType = 96;
constexpr int MaximumDynamicPayloadType = 127;

void writeU32(std::vector<std::uint8_t>& bytes,
              std::size_t offset,
              std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::uint8_t>(value >> 24);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

::media::Result<std::size_t> payloadOffset(
    std::span<const std::uint8_t> datagram)
{
    const std::size_t csrcBytes =
        static_cast<std::size_t>(datagram[0] & 0x0F) * 4;
    if (csrcBytes > datagram.size() - FixedHeaderBytes) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("RTP CSRC list is truncated"));
    }
    std::size_t offset = FixedHeaderBytes + csrcBytes;
    if ((datagram[0] & 0x10) == 0) {
        return ::media::Result<std::size_t>::success(offset);
    }
    if (datagram.size() - offset < 4) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("RTP extension header is truncated"));
    }
    const std::size_t extensionWords =
        (static_cast<std::size_t>(datagram[offset + 2]) << 8) |
        static_cast<std::size_t>(datagram[offset + 3]);
    const std::size_t extensionBytes = extensionWords * 4;
    offset += 4;
    if (extensionBytes > datagram.size() - offset) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("RTP extension data is truncated"));
    }
    return ::media::Result<std::size_t>::success(offset + extensionBytes);
}

} // namespace

MediaRtpDatagramRewriteIdentity::MediaRtpDatagramRewriteIdentity(
    int payloadType,
    std::uint32_t ssrc) noexcept
    : m_payloadType(payloadType),
      m_ssrc(ssrc)
{
}

::media::Result<MediaRtpDatagramRewriteIdentity>
MediaRtpDatagramRewriteIdentity::create(
    int payloadType,
    std::uint32_t ssrc) noexcept
{
    if (payloadType < MinimumDynamicPayloadType ||
        payloadType > MaximumDynamicPayloadType) {
        return ::media::Result<MediaRtpDatagramRewriteIdentity>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP output requires a dynamic payload type from 96 through 127"));
    }
    if (ssrc == 0) {
        return ::media::Result<MediaRtpDatagramRewriteIdentity>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP output requires a non-zero planned SSRC"));
    }
    return ::media::Result<MediaRtpDatagramRewriteIdentity>::success(
        MediaRtpDatagramRewriteIdentity(payloadType, ssrc));
}

MediaRtpDatagramRewriteParameters::MediaRtpDatagramRewriteParameters(
    MediaRtpDatagramRewriteIdentity identity,
    MediaRtpTimestamp timestamp) noexcept
    : m_identity(identity),
      m_timestamp(timestamp)
{
}

::media::Result<MediaRtpDatagramRewriteResult> MediaRtpDatagramRewriter::rewrite(
    std::span<const std::uint8_t> datagram,
    const MediaRtpDatagramRewriteParameters& parameters,
    std::vector<std::uint8_t>& output)
{
    output.clear();
    if (datagram.size() < FixedHeaderBytes) {
        return ::media::Result<MediaRtpDatagramRewriteResult>::failure(
            ::media::ErrorInfo::invalidArgument("RTP datagram is shorter than its fixed header"));
    }
    if ((datagram[0] >> 6) != 2) {
        return ::media::Result<MediaRtpDatagramRewriteResult>::failure(
            ::media::ErrorInfo::invalidArgument("RTP datagram version is not 2"));
    }
    if (datagram[1] >= 192 && datagram[1] <= 223) {
        return ::media::Result<MediaRtpDatagramRewriteResult>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP is forbidden on the scheduled RTP packetization boundary"));
    }
    if (static_cast<int>(datagram[1] & 0x7F) != parameters.identity().payloadType()) {
        return ::media::Result<MediaRtpDatagramRewriteResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg RTP payload type differs from the planned dynamic payload type"));
    }

    auto offset = payloadOffset(datagram);
    if (!offset) {
        return ::media::Result<MediaRtpDatagramRewriteResult>::failure(offset.error());
    }
    std::size_t payloadEnd = datagram.size();
    if ((datagram[0] & 0x20) != 0) {
        const std::size_t paddingBytes = datagram.back();
        if (paddingBytes == 0 || paddingBytes > payloadEnd - offset.value()) {
            return ::media::Result<MediaRtpDatagramRewriteResult>::failure(
                ::media::ErrorInfo::invalidArgument("RTP padding is malformed or truncated"));
        }
        payloadEnd -= paddingBytes;
    }
    if (payloadEnd <= offset.value()) {
        return ::media::Result<MediaRtpDatagramRewriteResult>::failure(
            ::media::ErrorInfo::invalidArgument("RTP datagram carries no media payload"));
    }

    output.resize(datagram.size());
    std::copy(datagram.begin(), datagram.end(), output.begin());
    output[1] = static_cast<std::uint8_t>(
        (output[1] & 0x80) | parameters.identity().payloadType());
    writeU32(output, 4, parameters.timestamp().wire());
    writeU32(output, 8, parameters.identity().ssrc());
    return ::media::Result<MediaRtpDatagramRewriteResult>::success(
        MediaRtpDatagramRewriteResult(payloadEnd - offset.value()));
}

} // namespace media::ffmpeg::graph

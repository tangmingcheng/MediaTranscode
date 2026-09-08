#include "internal/graph/protocol/rtp/MediaRtpWirePacketComposer.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

void writeU16(std::vector<std::uint8_t>& bytes,
              std::size_t offset,
              std::uint16_t value) noexcept
{
    bytes[offset] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

} // namespace

::media::Result<std::vector<std::uint8_t>>
MediaRtpWirePacketComposer::compose(
    std::span<const std::uint8_t> packetizedRtp,
    std::size_t expectedPayloadOctets,
    const MediaRtpDatagramRewriteIdentity& identity,
    MediaRtpTimestamp timestamp,
    std::uint16_t sequenceNumber,
    std::size_t maximumDatagramBytes)
{
    using Result = ::media::Result<std::vector<std::uint8_t>>;
    if (expectedPayloadOctets == 0 || packetizedRtp.size() > maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire packet exceeds its planned datagram or payload bound"));
    }
    std::vector<std::uint8_t> output;
    auto rewritten = MediaRtpDatagramRewriter::rewrite(
        packetizedRtp,
        MediaRtpDatagramRewriteParameters(identity, timestamp),
        output);
    if (!rewritten) return Result::failure(rewritten.error());
    if (rewritten.value().payloadOctets() != expectedPayloadOctets ||
        output.size() > maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP packetizer payload accounting differs from final wire bytes"));
    }
    writeU16(output, 2, sequenceNumber);
    return Result::success(std::move(output));
}

} // namespace media::ffmpeg::graph

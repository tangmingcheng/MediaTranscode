#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"

#include "internal/graph/protocol/mpegts/MediaTsTimestampFieldSerializer.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t TimestampModulus = std::uint64_t{1} << 33;

std::uint64_t wireValue(std::int64_t extended) noexcept
{
    const std::int64_t modulus = static_cast<std::int64_t>(TimestampModulus);
    const std::int64_t remainder = extended % modulus;
    return static_cast<std::uint64_t>(remainder < 0 ? remainder + modulus : remainder);
}

bool validClock(const MediaTsPacketClock& clock) noexcept
{
    return clock.wirePts < TimestampModulus && clock.wireDts < TimestampModulus &&
           clock.wirePts == wireValue(clock.extendedPts) &&
           clock.wireDts == wireValue(clock.extendedDts);
}

::media::Result<MediaTsPesHeader> invalid(const char* message)
{
    return ::media::Result<MediaTsPesHeader>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

} // namespace

::media::Result<MediaTsPesHeader> MediaTsPesSerializer::header(
    MediaScheduledStream stream,
    const MediaTsPacketClock& clock,
    std::size_t framedPayloadBytes)
{
    if (!validClock(clock) || clock.extendedDts > clock.extendedPts) {
        return invalid("MPEG-TS PES clock is inconsistent");
    }

    std::uint8_t streamId = 0;
    switch (stream) {
    case MediaScheduledStream::Video:
        streamId = 0xE0;
        break;
    case MediaScheduledStream::Audio:
        streamId = 0xC0;
        break;
    default:
        return invalid("MPEG-TS PES stream is invalid");
    }

    const bool hasDts = clock.extendedPts != clock.extendedDts;
    const std::size_t timestampBytes = hasDts ? 10 : 5;
    if (stream == MediaScheduledStream::Audio) {
        constexpr std::size_t FixedOptionalHeaderBytes = 3;
        if (framedPayloadBytes >
            std::numeric_limits<std::uint16_t>::max() -
                FixedOptionalHeaderBytes - timestampBytes) {
            return invalid("MPEG-TS audio PES length exceeds its 16-bit field");
        }
    }

    MediaTsPesHeader output;
    output.size = 9 + timestampBytes;
    output.bytes[2] = 0x01;
    output.bytes[3] = streamId;
    const std::size_t packetLength = stream == MediaScheduledStream::Video
        ? 0
        : 3 + timestampBytes + framedPayloadBytes;
    output.bytes[4] = static_cast<std::uint8_t>(packetLength >> 8);
    output.bytes[5] = static_cast<std::uint8_t>(packetLength);
    output.bytes[6] = 0x80;
    output.bytes[7] = hasDts ? 0xC0 : 0x80;
    output.bytes[8] = static_cast<std::uint8_t>(timestampBytes);

    auto pts = MediaTsTimestampFieldSerializer::serialize(
        hasDts ? 0x3 : 0x2, clock.wirePts);
    if (!pts) return invalid("MPEG-TS PES PTS cannot be serialized");
    std::copy(pts.value().begin(), pts.value().end(), output.bytes.begin() + 9);
    if (hasDts) {
        auto dts = MediaTsTimestampFieldSerializer::serialize(0x1, clock.wireDts);
        if (!dts) return invalid("MPEG-TS PES DTS cannot be serialized");
        std::copy(dts.value().begin(), dts.value().end(), output.bytes.begin() + 14);
    }
    return ::media::Result<MediaTsPesHeader>::success(std::move(output));
}

} // namespace media::ffmpeg::graph

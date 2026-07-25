#include "internal/graph/protocol/mpegts/MediaTsTimestampFieldSerializer.h"

namespace media::ffmpeg::graph {

::media::Result<std::array<std::uint8_t, 5>>
MediaTsTimestampFieldSerializer::serialize(std::uint8_t prefix,
                                           std::uint64_t wireTimestamp)
{
    constexpr std::uint64_t TimestampModulus = std::uint64_t{1} << 33;
    if ((prefix != 0x1 && prefix != 0x2 && prefix != 0x3) ||
        wireTimestamp >= TimestampModulus) {
        return ::media::Result<std::array<std::uint8_t, 5>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS timestamp field contract is invalid"));
    }
    return ::media::Result<std::array<std::uint8_t, 5>>::success({
        static_cast<std::uint8_t>((prefix << 4) |
                                  (((wireTimestamp >> 30) & 0x07) << 1) | 0x01),
        static_cast<std::uint8_t>(wireTimestamp >> 22),
        static_cast<std::uint8_t>((((wireTimestamp >> 15) & 0x7F) << 1) | 0x01),
        static_cast<std::uint8_t>(wireTimestamp >> 7),
        static_cast<std::uint8_t>(((wireTimestamp & 0x7F) << 1) | 0x01)});
}

} // namespace media::ffmpeg::graph

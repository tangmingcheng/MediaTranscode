#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"

#include <limits>
#include <new>

namespace media::ffmpeg::graph {

::media::Result<MediaAacRtpAuHeaderPlan>
MediaAacRtpAuHeaderPlanner::plan(std::span<const std::uint8_t> payload)
{
    using Result = ::media::Result<MediaAacRtpAuHeaderPlan>;
    if (payload.size() < 4) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC MPEG4-GENERIC RTP payload is truncated"));
    }
    const std::size_t headerBits =
        (static_cast<std::size_t>(payload[0]) << 8) | payload[1];
    if (headerBits == 0 || (headerBits % 16) != 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC MPEG4-GENERIC AU headers must use complete planned 16-bit headers"));
    }
    const std::size_t headerCount = headerBits / 16;
    const std::size_t headerBytes = headerBits / 8;
    if (headerBytes > payload.size() - 2) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC MPEG4-GENERIC AU header section is truncated"));
    }

    MediaAacRtpAuHeaderPlan result;
    result.payloadOffset = 2 + headerBytes;
    result.payloadBytes = payload.size() - result.payloadOffset;
    try {
        result.accessUnits.reserve(headerCount);
        for (std::size_t index = 0; index < headerCount; ++index) {
            const std::size_t offset = 2 + index * 2;
            const std::uint16_t bits = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(payload[offset]) << 8) |
                payload[offset + 1]);
            const std::size_t auSize = bits >> 3;
            const std::uint8_t indexField =
                static_cast<std::uint8_t>(bits & 0x07);
            if (auSize == 0) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "AAC MPEG4-GENERIC AU size is zero"));
            }
            std::uint64_t auIndex = indexField;
            if (!result.accessUnits.empty()) {
                const std::uint64_t increment =
                    static_cast<std::uint64_t>(indexField) + 1;
                if (result.accessUnits.back().index >
                    (std::numeric_limits<std::uint64_t>::max)() - increment) {
                    return Result::failure(::media::ErrorInfo::invalidArgument(
                        "AAC MPEG4-GENERIC expanded AU index overflows"));
                }
                auIndex = result.accessUnits.back().index + increment;
            }
            if (auSize > (std::numeric_limits<std::size_t>::max)() -
                    result.totalAccessUnitBytes) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "AAC MPEG4-GENERIC aggregate size overflows"));
            }
            result.totalAccessUnitBytes += auSize;
            result.accessUnits.push_back({auSize, auIndex});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "AAC MPEG4-GENERIC AU header plan"));
    }
    return Result::success(std::move(result));
}

} // namespace media::ffmpeg::graph

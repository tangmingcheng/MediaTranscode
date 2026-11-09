#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"

#include <limits>
#include <new>

namespace media::ffmpeg::graph {

::media::Result<MediaAacRtpAuHeaderPlan>
MediaAacRtpAuHeaderPlanner::plan(std::span<const std::uint8_t> payload, MediaLineageAllocationScope allocation)
try {
    using Result = ::media::Result<MediaAacRtpAuHeaderPlan>;
    using Layout = MediaAacRtpAuHeaderLayout;
    if (payload.size() < Layout::LengthBytes + Layout::HeaderBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC MPEG4-GENERIC RTP payload is truncated"));
    }
    const std::size_t headerBits =
        (static_cast<std::size_t>(payload[0]) << Layout::BitsPerByte) | payload[1];
    if (headerBits == 0 || (headerBits % Layout::HeaderBits) != 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC MPEG4-GENERIC AU headers must use complete planned 16-bit headers"));
    }
    const std::size_t headerCount = headerBits / Layout::HeaderBits;
    const std::size_t headerBytes = headerBits / Layout::BitsPerByte;
    if (headerBytes > payload.size() - Layout::LengthBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC MPEG4-GENERIC AU header section is truncated"));
    }

    MediaAacRtpAuHeaderPlan result(std::move(allocation));
    result.payloadOffset = Layout::LengthBytes + headerBytes;
    result.payloadBytes = payload.size() - result.payloadOffset;
    try {
        result.accessUnits.reserve(headerCount);
        for (std::size_t index = 0; index < headerCount; ++index) {
            const std::size_t offset = Layout::LengthBytes + index * Layout::HeaderBytes;
            const std::uint16_t bits = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(payload[offset]) << Layout::BitsPerByte) |
                payload[offset + 1]);
            const std::size_t auSize = bits >> Layout::IndexBits;
            const std::uint8_t indexField =
                static_cast<std::uint8_t>(bits & Layout::IndexMask);
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
            result.accessUnits.push_back(MediaAacRtpAuHeader{auSize, auIndex});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "AAC MPEG4-GENERIC AU header plan"));
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaAacRtpAuHeaderPlan>::failure(
        ::media::ErrorInfo::allocationFailed("AAC header owner allocation failed"));
}

} // namespace media::ffmpeg::graph

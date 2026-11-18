#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/storage/MediaLineageVector.h"

#include <algorithm>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

// RFC 3640 AAC-hbr layout supported by the factory. Parsed header storage
// covers malformed data too; completed AUs additionally require payload bytes.
struct MediaAacRtpAuHeaderLayout final {
    static constexpr int SizeBits = 13;
    static constexpr int IndexBits = 3;
    static constexpr int IndexDeltaBits = IndexBits;
    static constexpr std::size_t BitsPerByte = 8;
    static constexpr std::size_t LengthBytes = sizeof(std::uint16_t);
    static constexpr std::size_t HeaderBits = SizeBits + IndexBits;
    static constexpr std::size_t HeaderBytes = HeaderBits / BitsPerByte;
    static constexpr std::uint64_t MinimumAuBytes = 1;
    static constexpr std::uint64_t MaximumAuBytes = (std::uint64_t{1} << SizeBits) - 1;
    static constexpr std::uint16_t IndexMask = (std::uint16_t{1} << IndexBits) - 1;
    static constexpr std::uint64_t MaximumHeaders =
        (std::numeric_limits<std::uint16_t>::max)() / HeaderBits;

    static constexpr std::uint64_t maximumParsedHeaders(std::uint64_t payloadBytes) noexcept
    {
        return payloadBytes < LengthBytes ? 0 :
            (std::min)(MaximumHeaders, (payloadBytes - LengthBytes) / HeaderBytes);
    }
    static constexpr std::uint64_t maximumCompleteAccessUnits(std::uint64_t payloadBytes) noexcept
    {
        return payloadBytes < LengthBytes ? 0 :
            (std::min)(MaximumHeaders, (payloadBytes - LengthBytes) / (HeaderBytes + MinimumAuBytes));
    }
};

struct MediaAacRtpAuHeader final {
    std::size_t size = 0;
    std::uint64_t index = 0;
};

struct MediaAacRtpAuHeaderPlan final {
    explicit MediaAacRtpAuHeaderPlan(MediaLineageAllocationScope allocation)
        : accessUnits(MediaLineageAllocator<MediaAacRtpAuHeader>{std::move(allocation)}) {}
    std::size_t payloadOffset = 0;
    std::size_t payloadBytes = 0;
    std::size_t totalAccessUnitBytes = 0;
    MediaLineageVector<MediaAacRtpAuHeader> accessUnits;
};

class MediaAacRtpAuHeaderPlanner final {
public:
    static ::media::Result<MediaAacRtpAuHeaderPlan> plan(
        std::span<const std::uint8_t> payload, MediaLineageAllocationScope allocation);

private:
    MediaAacRtpAuHeaderPlanner() = delete;
};

} // namespace media::ffmpeg::graph

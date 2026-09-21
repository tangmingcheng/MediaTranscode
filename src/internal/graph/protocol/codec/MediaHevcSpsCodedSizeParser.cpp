#include "internal/graph/protocol/codec/MediaHevcSpsCodedSizeParser.h"

#include "internal/graph/protocol/codec/MediaSpsVuiParser.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

using BitReader = MediaRbspBitReader;

::media::Result<MediaSize> invalidSps()
{
    return ::media::Result<MediaSize>::failure(
        ::media::ErrorInfo::invalidArgument(
            "HEVC SPS does not contain a valid coded size"));
}

bool skipProfileTierLevel(BitReader& reader,
                          std::uint32_t maximumSubLayersMinusOne) noexcept
{
    if (!reader.skip(2 + 1 + 5 + 32 + 48 + 8)) return false;
    std::uint32_t profilePresent[8]{};
    std::uint32_t levelPresent[8]{};
    for (std::uint32_t index = 0; index < maximumSubLayersMinusOne; ++index) {
        if (!reader.bit(profilePresent[index]) ||
            !reader.bit(levelPresent[index])) return false;
    }
    if (maximumSubLayersMinusOne > 0 &&
        !reader.skip((8 - maximumSubLayersMinusOne) * 2)) return false;
    for (std::uint32_t index = 0; index < maximumSubLayersMinusOne; ++index) {
        if (profilePresent[index] && !reader.skip(2 + 1 + 5 + 32 + 48)) return false;
        if (levelPresent[index] && !reader.skip(8)) return false;
    }
    return true;
}

} // namespace

::media::Result<MediaSize> MediaHevcSpsCodedSizeParser::parse(
    std::span<const std::uint8_t> sps,
    std::optional<MediaVideoColorRangeFact>* colorRange)
{
    if (colorRange) colorRange->reset();
    if (sps.size() < 5 || ((sps[0] >> 1) & 0x3f) != 33) return invalidSps();
    BitReader reader(sps.subspan(2));
    std::uint32_t ignored = 0;
    std::uint32_t maximumSubLayersMinusOne = 0;
    if (!reader.bits(4, ignored) ||
        !reader.bits(3, maximumSubLayersMinusOne) || maximumSubLayersMinusOne > 6 ||
        !reader.bit(ignored) || (maximumSubLayersMinusOne == 0 && ignored == 0) ||
        !skipProfileTierLevel(reader, maximumSubLayersMinusOne) ||
        !reader.ue(ignored, 15)) return invalidSps();

    std::uint32_t chromaFormat = 0;
    std::uint32_t separateColourPlane = 0;
    if (!reader.ue(chromaFormat) || chromaFormat > 3) return invalidSps();
    if (chromaFormat == 3 && !reader.bit(separateColourPlane)) return invalidSps();
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!reader.ue(width) || !reader.ue(height) || width == 0 || height == 0) {
        return invalidSps();
    }
    std::uint32_t conformanceWindow = 0;
    if (!reader.bit(conformanceWindow)) return invalidSps();
    std::uint32_t left = 0, right = 0, top = 0, bottom = 0;
    if (conformanceWindow &&
        (!reader.ue(left) || !reader.ue(right) ||
         !reader.ue(top) || !reader.ue(bottom))) return invalidSps();

    const std::uint32_t chromaArrayType = separateColourPlane ? 0 : chromaFormat;
    const std::uint64_t subWidth = chromaArrayType == 1 || chromaArrayType == 2 ? 2 : 1;
    const std::uint64_t subHeight = chromaArrayType == 1 ? 2 : 1;
    const std::uint64_t croppedWidth = subWidth * (std::uint64_t{left} + right);
    const std::uint64_t croppedHeight = subHeight * (std::uint64_t{top} + bottom);
    if (croppedWidth >= width || croppedHeight >= height ||
        width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return invalidSps();
    }
    if (colorRange && (sps[0] & 0x81U) == 0 && (sps[1] & 0xf8U) == 0 && (sps[1] & 7U) == 1)
        *colorRange = MediaSpsVuiParser::hevc(reader, maximumSubLayersMinusOne);
    return ::media::Result<MediaSize>::success(MediaSize{
        static_cast<int>(width - croppedWidth),
        static_cast<int>(height - croppedHeight)});
}

} // namespace media::ffmpeg::graph

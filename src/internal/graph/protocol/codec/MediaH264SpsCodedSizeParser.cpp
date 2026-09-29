#include "internal/graph/protocol/codec/MediaH264SpsCodedSizeParser.h"

#include "internal/graph/protocol/codec/MediaSpsVuiParser.h"

#include <array>

#include <limits>

namespace media::ffmpeg::graph {
namespace {

using BitReader = MediaRbspBitReader;

bool skipScalingList(BitReader& reader, int count) noexcept
{
    int lastScale = 8;
    int nextScale = 8;
    for (int index = 0; index < count; ++index) {
        if (nextScale != 0) {
            std::int32_t delta = 0;
            if (!reader.se(delta, -128, 127)) return false;
            nextScale = (lastScale + delta + 256) % 256;
        }
        if (nextScale != 0) lastScale = nextScale;
    }
    return true;
}

bool highProfile(std::uint32_t profile) noexcept
{
    constexpr std::array<std::uint32_t, 13> profiles{
        100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135};
    for (const auto candidate : profiles) {
        if (candidate == profile) return true;
    }
    return false;
}

::media::Result<MediaSize> invalidSps()
{
    return ::media::Result<MediaSize>::failure(
        ::media::ErrorInfo::invalidArgument(
            "H264 SPS does not contain a valid coded size"));
}

} // namespace

::media::Result<MediaSize> MediaH264SpsCodedSizeParser::parse(
    std::span<const std::uint8_t> sps,
    std::optional<MediaVideoColorRangeFact>* colorRange)
{
    if (colorRange) colorRange->reset();
    if (sps.size() < 4 || (sps[0] & 0x1f) != 7) return invalidSps();
    BitReader reader(sps.subspan(1));
    std::uint32_t profile = 0;
    std::uint32_t ignored = 0;
    if (!reader.bits(8, profile) || !reader.bits(8, ignored) || (ignored & 3U) != 0 ||
        !reader.bits(8, ignored) || !reader.ue(ignored, 31)) return invalidSps();

    std::uint32_t chromaFormat = profile == 183 ? 0 : 1;
    std::uint32_t separateColourPlane = 0;
    if (highProfile(profile)) {
        if (!reader.ue(chromaFormat) || chromaFormat > 3) return invalidSps();
        if (chromaFormat == 3 && !reader.bit(separateColourPlane)) return invalidSps();
        if (!reader.ue(ignored, 6) || !reader.ue(ignored, 6) || !reader.bit(ignored)) return invalidSps();
        std::uint32_t scalingPresent = 0;
        if (!reader.bit(scalingPresent)) return invalidSps();
        if (scalingPresent) {
            const int count = chromaFormat == 3 ? 12 : 8;
            for (int index = 0; index < count; ++index) {
                std::uint32_t present = 0;
                if (!reader.bit(present)) return invalidSps();
                if (present && !skipScalingList(reader, index < 6 ? 16 : 64)) return invalidSps();
            }
        }
    }
    if (!reader.ue(ignored, 12)) return invalidSps();
    std::uint32_t picOrderCountType = 0;
    if (!reader.ue(picOrderCountType) || picOrderCountType > 2) return invalidSps();
    if (picOrderCountType == 0) {
        if (!reader.ue(ignored, 12)) return invalidSps();
    } else if (picOrderCountType == 1) {
        if (!reader.bit(ignored)) return invalidSps();
        std::int32_t signedIgnored = 0;
        if (!reader.se(signedIgnored) || !reader.se(signedIgnored)) return invalidSps();
        std::uint32_t cycle = 0;
        if (!reader.ue(cycle) || cycle > 255) return invalidSps();
        for (std::uint32_t index = 0; index < cycle; ++index) {
            if (!reader.se(signedIgnored)) return invalidSps();
        }
    }
    if (!reader.ue(ignored, 16) || !reader.bit(ignored)) return invalidSps();
    std::uint32_t widthMbsMinusOne = 0;
    std::uint32_t heightMapUnitsMinusOne = 0;
    std::uint32_t frameMbsOnly = 0;
    if (!reader.ue(widthMbsMinusOne) || !reader.ue(heightMapUnitsMinusOne) ||
        !reader.bit(frameMbsOnly)) return invalidSps();
    if (!frameMbsOnly && !reader.bit(ignored)) return invalidSps();
    if (!reader.bit(ignored)) return invalidSps();
    std::uint32_t cropping = 0;
    if (!reader.bit(cropping)) return invalidSps();
    std::uint32_t cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
    if (cropping && (!reader.ue(cropLeft) || !reader.ue(cropRight) ||
                     !reader.ue(cropTop) || !reader.ue(cropBottom))) return invalidSps();

    const std::uint32_t effectiveChroma = separateColourPlane ? 0 : chromaFormat;
    const std::uint32_t subWidth = effectiveChroma == 1 || effectiveChroma == 2 ? 2 : 1;
    const std::uint32_t subHeight = effectiveChroma == 1 ? 2 : 1;
    const std::uint64_t cropUnitX = effectiveChroma == 0 ? 1 : subWidth;
    const std::uint64_t cropUnitY = (effectiveChroma == 0 ? 1 : subHeight) * (2 - frameMbsOnly);
    const std::uint64_t codedWidth = (std::uint64_t{widthMbsMinusOne} + 1) * 16;
    const std::uint64_t codedHeight = (std::uint64_t{heightMapUnitsMinusOne} + 1) * 16 * (2 - frameMbsOnly);
    const std::uint64_t croppedWidth = cropUnitX * (std::uint64_t{cropLeft} + cropRight);
    const std::uint64_t croppedHeight = cropUnitY * (std::uint64_t{cropTop} + cropBottom);
    if (croppedWidth >= codedWidth || croppedHeight >= codedHeight ||
        codedWidth > std::numeric_limits<int>::max() ||
        codedHeight > std::numeric_limits<int>::max()) return invalidSps();
    if (colorRange && (sps[0] & 0x80U) == 0 && (sps[0] & 0x60U) != 0 &&
        (highProfile(profile) || profile == 66 || profile == 77 || profile == 88 || profile == 183))
        *colorRange = MediaSpsVuiParser::h264(reader);
    return ::media::Result<MediaSize>::success(MediaSize{
        static_cast<int>(codedWidth - croppedWidth),
        static_cast<int>(codedHeight - croppedHeight)});
}

} // namespace media::ffmpeg::graph

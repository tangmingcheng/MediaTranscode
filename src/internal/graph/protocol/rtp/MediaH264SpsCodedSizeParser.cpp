#include "internal/graph/protocol/rtp/MediaH264SpsCodedSizeParser.h"

#include <array>
#include <limits>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

class BitReader final {
public:
    explicit BitReader(std::vector<std::uint8_t> bytes)
        : m_bytes(std::move(bytes))
    {
    }

    bool bit(std::uint32_t& value) noexcept
    {
        if (m_offset >= m_bytes.size() * 8) return false;
        value = (m_bytes[m_offset / 8] >> (7 - (m_offset % 8))) & 1U;
        ++m_offset;
        return true;
    }

    bool bits(unsigned count, std::uint32_t& value) noexcept
    {
        if (count > 32) return false;
        value = 0;
        for (unsigned index = 0; index < count; ++index) {
            std::uint32_t next = 0;
            if (!bit(next)) return false;
            value = (value << 1) | next;
        }
        return true;
    }

    bool ue(std::uint32_t& value) noexcept
    {
        unsigned leadingZeros = 0;
        std::uint32_t next = 0;
        while (bit(next) && next == 0) {
            if (++leadingZeros > 31) return false;
        }
        if (next == 0) return false;
        std::uint32_t suffix = 0;
        if (leadingZeros && !bits(leadingZeros, suffix)) return false;
        const std::uint64_t decoded = ((std::uint64_t{1} << leadingZeros) - 1) + suffix;
        if (decoded > std::numeric_limits<std::uint32_t>::max()) return false;
        value = static_cast<std::uint32_t>(decoded);
        return true;
    }

    bool se(std::int32_t& value) noexcept
    {
        std::uint32_t code = 0;
        if (!ue(code)) return false;
        value = (code & 1U) ? static_cast<std::int32_t>((code + 1) / 2)
                            : -static_cast<std::int32_t>(code / 2);
        return true;
    }

private:
    std::vector<std::uint8_t> m_bytes;
    std::size_t m_offset = 0;
};

bool skipScalingList(BitReader& reader, int count) noexcept
{
    int lastScale = 8;
    int nextScale = 8;
    for (int index = 0; index < count; ++index) {
        if (nextScale != 0) {
            std::int32_t delta = 0;
            if (!reader.se(delta)) return false;
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

std::vector<std::uint8_t> rbsp(std::span<const std::uint8_t> sps)
{
    std::vector<std::uint8_t> output;
    output.reserve(sps.size() - 1);
    unsigned zeroCount = 0;
    for (std::size_t index = 1; index < sps.size(); ++index) {
        const std::uint8_t byte = sps[index];
        if (zeroCount >= 2 && byte == 3) {
            zeroCount = 0;
            continue;
        }
        output.push_back(byte);
        zeroCount = byte == 0 ? zeroCount + 1 : 0;
    }
    return output;
}

::media::Result<MediaSize> invalidSps()
{
    return ::media::Result<MediaSize>::failure(
        ::media::ErrorInfo::invalidArgument(
            "H264 SPS does not contain a valid coded size"));
}

} // namespace

::media::Result<MediaSize> MediaH264SpsCodedSizeParser::parse(
    std::span<const std::uint8_t> sps)
{
    if (sps.size() < 4 || (sps[0] & 0x1f) != 7) return invalidSps();
    BitReader reader(rbsp(sps));
    std::uint32_t profile = 0;
    std::uint32_t ignored = 0;
    if (!reader.bits(8, profile) || !reader.bits(8, ignored) ||
        !reader.bits(8, ignored) || !reader.ue(ignored)) return invalidSps();

    std::uint32_t chromaFormat = 1;
    std::uint32_t separateColourPlane = 0;
    if (highProfile(profile)) {
        if (!reader.ue(chromaFormat) || chromaFormat > 3) return invalidSps();
        if (chromaFormat == 3 && !reader.bit(separateColourPlane)) return invalidSps();
        if (!reader.ue(ignored) || !reader.ue(ignored) || !reader.bit(ignored)) return invalidSps();
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
    if (!reader.ue(ignored)) return invalidSps();
    std::uint32_t picOrderCountType = 0;
    if (!reader.ue(picOrderCountType) || picOrderCountType > 2) return invalidSps();
    if (picOrderCountType == 0) {
        if (!reader.ue(ignored)) return invalidSps();
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
    if (!reader.ue(ignored) || !reader.bit(ignored)) return invalidSps();
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
    const std::uint64_t croppedWidth = cropUnitX * (cropLeft + cropRight);
    const std::uint64_t croppedHeight = cropUnitY * (cropTop + cropBottom);
    if (croppedWidth >= codedWidth || croppedHeight >= codedHeight ||
        codedWidth > std::numeric_limits<int>::max() ||
        codedHeight > std::numeric_limits<int>::max()) return invalidSps();
    return ::media::Result<MediaSize>::success(MediaSize{
        static_cast<int>(codedWidth - croppedWidth),
        static_cast<int>(codedHeight - croppedHeight)});
}

} // namespace media::ffmpeg::graph

#include "internal/graph/protocol/rtp/MediaHevcSpsCodedSizeParser.h"

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

    bool skip(std::size_t count) noexcept
    {
        if (count > m_bytes.size() * 8 - m_offset) return false;
        m_offset += count;
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

private:
    std::vector<std::uint8_t> m_bytes;
    std::size_t m_offset = 0;
};

std::vector<std::uint8_t> rbsp(std::span<const std::uint8_t> sps)
{
    std::vector<std::uint8_t> output;
    output.reserve(sps.size() - 2);
    unsigned zeroCount = 0;
    for (std::size_t index = 2; index < sps.size(); ++index) {
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
    std::span<const std::uint8_t> sps)
{
    if (sps.size() < 5 || ((sps[0] >> 1) & 0x3f) != 33) return invalidSps();
    BitReader reader(rbsp(sps));
    std::uint32_t ignored = 0;
    std::uint32_t maximumSubLayersMinusOne = 0;
    if (!reader.bits(4, ignored) ||
        !reader.bits(3, maximumSubLayersMinusOne) ||
        !reader.bit(ignored) ||
        !skipProfileTierLevel(reader, maximumSubLayersMinusOne) ||
        !reader.ue(ignored)) return invalidSps();

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
    const std::uint64_t croppedWidth = subWidth * (left + right);
    const std::uint64_t croppedHeight = subHeight * (top + bottom);
    if (croppedWidth >= width || croppedHeight >= height ||
        width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return invalidSps();
    }
    return ::media::Result<MediaSize>::success(MediaSize{
        static_cast<int>(width - croppedWidth),
        static_cast<int>(height - croppedHeight)});
}

} // namespace media::ffmpeg::graph

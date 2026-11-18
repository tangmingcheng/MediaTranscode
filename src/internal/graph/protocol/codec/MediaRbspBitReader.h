#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

// H.264/H.265 EBSP removal and bounded Exp-Golomb reads shared by SPS parsers.
class MediaRbspBitReader final {
public:
    explicit MediaRbspBitReader(std::span<const std::uint8_t> bytes)
    {
        m_bytes.reserve(bytes.size());
        unsigned zeros = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const auto byte = bytes[i];
            if (zeros == 2 && byte == 3) {
                if (i + 1 == bytes.size() || bytes[i + 1] > 3) m_valid = false;
                zeros = 0;
                continue;
            }
            if (zeros == 2 && byte < 3) m_valid = false;
            m_bytes.push_back(byte);
            zeros = byte == 0 ? zeros + 1 : 0;
        }
    }
    bool bit(std::uint32_t& value) noexcept { return bits(1, value); }
    bool bits(unsigned count, std::uint32_t& value) noexcept
    {
        if (!m_valid || count > 32 || count > m_bytes.size() * 8 - m_offset) return false;
        value = 0;
        for (unsigned i = 0; i < count; ++i, ++m_offset)
            value = (value << 1) | ((m_bytes[m_offset / 8] >> (7 - m_offset % 8)) & 1U);
        return true;
    }
    bool skip(std::size_t count) noexcept
    {
        if (!m_valid || count > m_bytes.size() * 8 - m_offset) return false;
        m_offset += count;
        return true;
    }
    bool ue(std::uint32_t& value, std::uint32_t maximum = UINT32_MAX) noexcept
    {
        unsigned zeros = 0;
        std::uint32_t next = 0;
        do {
            if (!bit(next)) return false;
            if (next) break;
            if (++zeros > 31) return false;
        } while (true);
        std::uint32_t suffix = 0;
        if (!bits(zeros, suffix)) return false;
        value = static_cast<std::uint32_t>((std::uint64_t{1} << zeros) - 1 + suffix);
        return value <= maximum;
    }
    bool se(std::int32_t& value, std::int32_t minimum = INT32_MIN,
            std::int32_t maximum = INT32_MAX) noexcept
    {
        std::uint32_t code = 0;
        if (!ue(code)) return false;
        const auto decoded = (code & 1U) ? (std::int64_t{code} + 1) / 2 : -std::int64_t{code} / 2;
        if (decoded < minimum || decoded > maximum) return false;
        value = static_cast<std::int32_t>(decoded);
        return true;
    }
    bool trailingBits() noexcept
    {
        std::uint32_t value = 0;
        if (!bit(value) || value != 1) return false;
        while (m_offset % 8) if (!bit(value) || value != 0) return false;
        return m_offset == m_bytes.size() * 8;
    }
private:
    std::vector<std::uint8_t> m_bytes;
    std::size_t m_offset = 0;
    bool m_valid = true;
};

} // namespace media::ffmpeg::graph

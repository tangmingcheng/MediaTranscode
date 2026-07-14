#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaVideoRepeatRequestId final {
public:
    explicit constexpr MediaVideoRepeatRequestId(std::uint64_t value) noexcept
        : m_value(value) {}

    constexpr std::uint64_t value() const noexcept { return m_value; }

    friend constexpr bool operator==(MediaVideoRepeatRequestId,
                                     MediaVideoRepeatRequestId) noexcept = default;

private:
    std::uint64_t m_value;
};

} // namespace media::ffmpeg::graph

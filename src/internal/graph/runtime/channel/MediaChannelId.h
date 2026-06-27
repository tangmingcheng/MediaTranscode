#pragma once

#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

struct MediaChannelId {
    uint32_t value = invalidValue();

    static constexpr uint32_t invalidValue()
    {
        return std::numeric_limits<uint32_t>::max();
    }

    static constexpr MediaChannelId invalid()
    {
        return MediaChannelId{};
    }

    static constexpr MediaChannelId fromValue(uint32_t id)
    {
        return MediaChannelId{ id };
    }

    constexpr bool isValid() const noexcept
    {
        return value != invalidValue();
    }

    explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }
};

constexpr bool operator==(MediaChannelId lhs, MediaChannelId rhs) noexcept
{
    return lhs.value == rhs.value;
}

constexpr bool operator!=(MediaChannelId lhs, MediaChannelId rhs) noexcept
{
    return !(lhs == rhs);
}

constexpr bool operator<(MediaChannelId lhs, MediaChannelId rhs) noexcept
{
    return lhs.value < rhs.value;
}

} // namespace media::ffmpeg::graph

#pragma once

#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

struct MediaNodeId {
    uint32_t value = invalidValue();

    static constexpr uint32_t invalidValue()
    {
        return std::numeric_limits<uint32_t>::max();
    }

    static constexpr MediaNodeId invalid()
    {
        return MediaNodeId{};
    }

    static constexpr MediaNodeId fromValue(uint32_t id)
    {
        return MediaNodeId{ id };
    }

    constexpr bool isValid() const
    {
        return value != invalidValue();
    }

    explicit constexpr operator bool() const
    {
        return isValid();
    }
};

struct MediaPortId {
    uint32_t value = invalidValue();

    static constexpr uint32_t invalidValue()
    {
        return std::numeric_limits<uint32_t>::max();
    }

    static constexpr MediaPortId invalid()
    {
        return MediaPortId{};
    }

    static constexpr MediaPortId fromValue(uint32_t id)
    {
        return MediaPortId{ id };
    }

    constexpr bool isValid() const
    {
        return value != invalidValue();
    }

    explicit constexpr operator bool() const
    {
        return isValid();
    }
};

struct MediaEdgeId {
    uint32_t value = invalidValue();

    static constexpr uint32_t invalidValue()
    {
        return std::numeric_limits<uint32_t>::max();
    }

    static constexpr MediaEdgeId invalid()
    {
        return MediaEdgeId{};
    }

    static constexpr MediaEdgeId fromValue(uint32_t id)
    {
        return MediaEdgeId{ id };
    }

    constexpr bool isValid() const
    {
        return value != invalidValue();
    }

    explicit constexpr operator bool() const
    {
        return isValid();
    }
};

constexpr bool operator==(MediaNodeId lhs, MediaNodeId rhs)
{
    return lhs.value == rhs.value;
}

constexpr bool operator!=(MediaNodeId lhs, MediaNodeId rhs)
{
    return !(lhs == rhs);
}

constexpr bool operator<(MediaNodeId lhs, MediaNodeId rhs)
{
    return lhs.value < rhs.value;
}

constexpr bool operator==(MediaPortId lhs, MediaPortId rhs)
{
    return lhs.value == rhs.value;
}

constexpr bool operator!=(MediaPortId lhs, MediaPortId rhs)
{
    return !(lhs == rhs);
}

constexpr bool operator<(MediaPortId lhs, MediaPortId rhs)
{
    return lhs.value < rhs.value;
}

constexpr bool operator==(MediaEdgeId lhs, MediaEdgeId rhs)
{
    return lhs.value == rhs.value;
}

constexpr bool operator!=(MediaEdgeId lhs, MediaEdgeId rhs)
{
    return !(lhs == rhs);
}

constexpr bool operator<(MediaEdgeId lhs, MediaEdgeId rhs)
{
    return lhs.value < rhs.value;
}

} // namespace media::ffmpeg::graph

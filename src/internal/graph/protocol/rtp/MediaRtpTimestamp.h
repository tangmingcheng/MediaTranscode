#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtpOutputClockMapper;

class MediaRtpTimestamp final {
public:
    std::uint64_t extendedTicks() const noexcept { return m_extendedTicks; }
    std::uint32_t wire() const noexcept { return m_wire; }

    friend bool operator==(MediaRtpTimestamp,
                           MediaRtpTimestamp) noexcept = default;

private:
    friend class MediaRtpOutputClockMapper;

    MediaRtpTimestamp(std::uint64_t extendedTicks,
                      std::uint32_t wire) noexcept
        : m_extendedTicks(extendedTicks), m_wire(wire)
    {
    }

    std::uint64_t m_extendedTicks;
    std::uint32_t m_wire;
};

} // namespace media::ffmpeg::graph

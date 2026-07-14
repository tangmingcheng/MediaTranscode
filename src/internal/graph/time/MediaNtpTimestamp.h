#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaSharedNtpEpoch;

struct MediaNtpWireTimestamp final {
    std::uint32_t seconds;
    std::uint32_t fraction;

    friend bool operator==(MediaNtpWireTimestamp,
                           MediaNtpWireTimestamp) noexcept = default;
};

class MediaNtpTimestamp final {
public:
    std::uint64_t seconds() const noexcept { return m_seconds; }
    std::uint32_t fraction() const noexcept { return m_fraction; }
    MediaNtpWireTimestamp wire() const noexcept
    {
        return MediaNtpWireTimestamp{
            static_cast<std::uint32_t>(m_seconds), m_fraction};
    }

    friend bool operator==(MediaNtpTimestamp,
                           MediaNtpTimestamp) noexcept = default;

private:
    friend class MediaSharedNtpEpoch;

    MediaNtpTimestamp(std::uint64_t seconds, std::uint32_t fraction) noexcept
        : m_seconds(seconds), m_fraction(fraction)
    {
    }

    std::uint64_t m_seconds;
    std::uint32_t m_fraction;
};

} // namespace media::ffmpeg::graph

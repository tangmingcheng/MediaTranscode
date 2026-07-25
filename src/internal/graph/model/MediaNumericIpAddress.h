#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"

#include <array>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

class MediaNumericIpAddress final {
public:
    static ::media::Result<MediaNumericIpAddress> create(
        MediaIpAddressFamily addressFamily, std::string presentation);

    MediaIpAddressFamily addressFamily() const noexcept { return m_addressFamily; }
    const std::string& presentation() const noexcept { return m_presentation; }
    bool isMulticast() const noexcept;

    friend bool operator==(
        const MediaNumericIpAddress& left,
        const MediaNumericIpAddress& right) noexcept;

private:
    MediaNumericIpAddress(
        MediaIpAddressFamily addressFamily,
        std::string presentation,
        std::array<std::uint8_t, 16> canonicalBytes) noexcept;

    MediaIpAddressFamily m_addressFamily;
    std::string m_presentation;
    std::array<std::uint8_t, 16> m_canonicalBytes;
};

} // namespace media::ffmpeg::graph

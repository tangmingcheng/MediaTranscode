#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/model/MediaNumericIpAddress.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

inline constexpr std::size_t kMediaUdpMaximumPayloadBytes = 65'507;

class MediaUdpDatagramEndpoint final {
public:
    static ::media::Result<MediaUdpDatagramEndpoint> create(
        MediaIpAddressFamily addressFamily,
        std::string numericAddress,
        std::uint16_t port);

    MediaIpAddressFamily addressFamily() const noexcept
    {
        return m_address.addressFamily();
    }
    const std::string& numericAddress() const noexcept
    {
        return m_address.presentation();
    }
    std::uint16_t port() const noexcept { return m_port; }

    friend bool operator==(const MediaUdpDatagramEndpoint&,
                           const MediaUdpDatagramEndpoint&) = default;

private:
    MediaUdpDatagramEndpoint(MediaNumericIpAddress address,
                             std::uint16_t port) noexcept;

    MediaNumericIpAddress m_address;
    std::uint16_t m_port;
};

} // namespace media::ffmpeg::graph

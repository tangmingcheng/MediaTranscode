#include "internal/graph/runtime/network/MediaUdpDatagramEndpoint.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaUdpDatagramEndpoint::MediaUdpDatagramEndpoint(
    MediaNumericIpAddress address,
    std::uint16_t port) noexcept
    : m_address(std::move(address)),
      m_port(port)
{
}

::media::Result<MediaUdpDatagramEndpoint> MediaUdpDatagramEndpoint::create(
    MediaIpAddressFamily addressFamily,
    std::string numericAddress,
    std::uint16_t port)
{
    auto address = MediaNumericIpAddress::create(
        addressFamily, std::move(numericAddress));
    if (!address) {
        return ::media::Result<MediaUdpDatagramEndpoint>::failure(
            address.error());
    }
    return ::media::Result<MediaUdpDatagramEndpoint>::success(
        MediaUdpDatagramEndpoint(std::move(address.value()), port));
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaRawRtpBootstrapPlan.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t MinimumRtpDatagramBytes = 12;
constexpr std::size_t Ipv4MaximumUdpPayloadBytes =
    65'535 - 20 - 8;
constexpr std::size_t Ipv6MaximumUdpPayloadBytes =
    65'535 - 8;
constexpr int MicrosecondsPerMillisecond = 1'000;

::media::Result<std::size_t> maximumUdpPayloadBytes(
    MediaIpAddressFamily addressFamily)
{
    switch (addressFamily) {
    case MediaIpAddressFamily::Ipv4:
        return ::media::Result<std::size_t>::success(
            Ipv4MaximumUdpPayloadBytes);
    case MediaIpAddressFamily::Ipv6:
        return ::media::Result<std::size_t>::success(
            Ipv6MaximumUdpPayloadBytes);
    default:
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::unsupported(
                "raw RTP bootstrap requires a supported IP address family"));
    }
}

} // namespace

MediaRawRtpBootstrapPlan::MediaRawRtpBootstrapPlan(
    int socketReceiveBufferBytes,
    std::size_t maximumDatagramBytes,
    std::size_t reorderWindowPackets,
    int maximumReorderDelayMilliseconds) noexcept
    : m_socketReceiveBufferBytes(socketReceiveBufferBytes),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_reorderWindowPackets(reorderWindowPackets),
      m_maximumReorderDelayMilliseconds(maximumReorderDelayMilliseconds)
{
}

::media::Result<MediaRawRtpBootstrapPlan>
MediaRawRtpBootstrapPlan::create(
    MediaIpAddressFamily addressFamily,
    std::size_t preparedInputByteBudget,
    int analyzeDurationMicroseconds)
{
    if (preparedInputByteBudget < MinimumRtpDatagramBytes ||
        preparedInputByteBudget >
            static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        analyzeDurationMicroseconds <= 0) {
        return ::media::Result<MediaRawRtpBootstrapPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP bootstrap requires directly bounded probe bytes and duration"));
    }
    auto protocolPayloadLimit = maximumUdpPayloadBytes(addressFamily);
    if (!protocolPayloadLimit) {
        return ::media::Result<MediaRawRtpBootstrapPlan>::failure(
            protocolPayloadLimit.error());
    }
    const int maximumReorderDelayMilliseconds =
        analyzeDurationMicroseconds / MicrosecondsPerMillisecond +
        (analyzeDurationMicroseconds % MicrosecondsPerMillisecond != 0 ? 1 : 0);
    MediaRawRtpBootstrapPlan product(
        static_cast<int>(preparedInputByteBudget),
        (std::min)(preparedInputByteBudget, protocolPayloadLimit.value()),
        preparedInputByteBudget / MinimumRtpDatagramBytes,
        maximumReorderDelayMilliseconds);
    if (auto status = product.validateProduct(); !status) {
        return ::media::Result<MediaRawRtpBootstrapPlan>::failure(
            status.error());
    }
    return ::media::Result<MediaRawRtpBootstrapPlan>::success(
        std::move(product));
}

int MediaRawRtpBootstrapPlan::socketReceiveBufferBytes() const noexcept
{
    return m_socketReceiveBufferBytes;
}

std::size_t MediaRawRtpBootstrapPlan::maximumDatagramBytes() const noexcept
{
    return m_maximumDatagramBytes;
}

std::size_t MediaRawRtpBootstrapPlan::reorderWindowPackets() const noexcept
{
    return m_reorderWindowPackets;
}

int MediaRawRtpBootstrapPlan::maximumReorderDelayMilliseconds() const noexcept
{
    return m_maximumReorderDelayMilliseconds;
}

::media::Status MediaRawRtpBootstrapPlan::validateProduct() const
{
    if (m_socketReceiveBufferBytes <= 0 ||
        m_maximumDatagramBytes < MinimumRtpDatagramBytes ||
        m_maximumDatagramBytes >
            static_cast<std::size_t>(m_socketReceiveBufferBytes) ||
        m_reorderWindowPackets == 0 ||
        m_maximumReorderDelayMilliseconds <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP bootstrap plan contains inconsistent bounds"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaRealtimeDatagramPayloadPlanner.h"

#include <algorithm>
#include <cstdint>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
constexpr std::uint64_t MaximumUdpPayloadBytes = 65'507;

} // namespace

::media::Result<MediaRealtimeDeploymentMtuFact>
MediaRealtimeDatagramPayloadPlanner::plan(
    const MediaDatagramRouteFact& route)
{
    const auto ipHeaderBytes =
        route.addressFamily == MediaIpAddressFamily::Ipv4
            ? Ipv4HeaderBytes
            : route.addressFamily == MediaIpAddressFamily::Ipv6
                ? Ipv6HeaderBytes
                : 0U;
    if (route.authority.empty() || ipHeaderBytes == 0 ||
        route.maximumIpPacketBytes <= ipHeaderBytes + UdpHeaderBytes) {
        return ::media::Result<MediaRealtimeDeploymentMtuFact>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime Datagram payload planning requires an authoritative route MTU"));
    }
    const auto routeMaximumPayloadBytes = (std::min)(
        MaximumUdpPayloadBytes,
        route.maximumIpPacketBytes - ipHeaderBytes - UdpHeaderBytes);
    return ::media::Result<MediaRealtimeDeploymentMtuFact>::success({
        route.addressFamily,
        route.authority,
        route.maximumIpPacketBytes,
        routeMaximumPayloadBytes});
}

::media::Result<std::uint64_t>
MediaRealtimeDatagramPayloadPlanner::maximumPayloadBytes(
    const MediaRealtimeDeploymentMtuFact& mtu)
{
    const auto ipHeaderBytes = mtu.addressFamily == MediaIpAddressFamily::Ipv4
        ? Ipv4HeaderBytes : mtu.addressFamily == MediaIpAddressFamily::Ipv6
            ? Ipv6HeaderBytes : 0U;
    if (mtu.authority.empty() || ipHeaderBytes == 0 ||
        mtu.senderMaximumPayloadBytes == 0 ||
        mtu.maximumIpPacketBytes <= ipHeaderBytes + UdpHeaderBytes) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime Datagram payload requires complete deployment MTU facts"));
    }
    return ::media::Result<std::uint64_t>::success((std::min)(
        mtu.senderMaximumPayloadBytes,
        mtu.maximumIpPacketBytes - ipHeaderBytes - UdpHeaderBytes));
}

} // namespace media::ffmpeg::graph

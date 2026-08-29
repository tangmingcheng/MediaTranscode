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
    const MediaDatagramRouteFact& route,
    std::uint64_t pathMaximumIpPacketBytes)
{
    const auto ipHeaderBytes =
        route.addressFamily == MediaIpAddressFamily::Ipv4
            ? Ipv4HeaderBytes
            : route.addressFamily == MediaIpAddressFamily::Ipv6
                ? Ipv6HeaderBytes
                : 0U;
    if (route.authority.empty() || ipHeaderBytes == 0 ||
        route.maximumIpPacketBytes <= ipHeaderBytes + UdpHeaderBytes ||
        pathMaximumIpPacketBytes <= ipHeaderBytes + UdpHeaderBytes ||
        pathMaximumIpPacketBytes > route.maximumIpPacketBytes) {
        return ::media::Result<MediaRealtimeDeploymentMtuFact>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime Datagram payload planning requires an authoritative route MTU"));
    }
    const auto routeMaximumPayloadBytes = (std::min)(
        MaximumUdpPayloadBytes,
        pathMaximumIpPacketBytes - ipHeaderBytes - UdpHeaderBytes);
    return ::media::Result<MediaRealtimeDeploymentMtuFact>::success({
        route.addressFamily,
        route.authority +
            "+caller-provisioned-path-mtu-validated-by-egress-interface",
        pathMaximumIpPacketBytes,
        routeMaximumPayloadBytes});
}

} // namespace media::ffmpeg::graph

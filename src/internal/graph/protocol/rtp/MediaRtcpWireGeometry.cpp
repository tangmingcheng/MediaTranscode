#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t> MediaRtcpWireGeometry::compoundPayloadBytes(
    std::size_t cnameBytes)
{
    constexpr std::uint64_t SenderReportBytes = 28;
    constexpr std::uint64_t SdesFixedBytes = 10;
    auto raw = MediaCheckedArithmetic::add(
        SdesFixedBytes, static_cast<std::uint64_t>(cnameBytes),
        "RTCP SDES bytes");
    auto padded = raw
        ? MediaCheckedArithmetic::ceilScale(
              raw.value(), 1, 4, "RTCP SDES words")
        : raw;
    auto paddedBytes = padded
        ? MediaCheckedArithmetic::multiply(
              padded.value(), 4, "RTCP padded SDES bytes")
        : padded;
    return paddedBytes
        ? MediaCheckedArithmetic::add(
              SenderReportBytes, paddedBytes.value(), "RTCP compound bytes")
        : paddedBytes;
}

::media::Result<std::uint64_t> MediaRtcpWireGeometry::compoundWireBytes(
    std::size_t cnameBytes, MediaIpAddressFamily addressFamily)
{
    constexpr std::uint64_t Ipv4HeaderBytes = 20;
    constexpr std::uint64_t Ipv6HeaderBytes = 40;
    constexpr std::uint64_t UdpHeaderBytes = 8;
    auto payload = compoundPayloadBytes(cnameBytes);
    const auto ipHeader = addressFamily == MediaIpAddressFamily::Ipv4
        ? Ipv4HeaderBytes
        : addressFamily == MediaIpAddressFamily::Ipv6
            ? Ipv6HeaderBytes : 0;
    return payload && ipHeader != 0
        ? MediaCheckedArithmetic::add(
              payload.value(), ipHeader + UdpHeaderBytes,
              "RTCP compound wire bytes")
        : ::media::Result<std::uint64_t>::failure(
              payload ? ::media::ErrorInfo::invalidArgument(
                            "RTCP wire geometry requires an IP address family")
                      : payload.error());
}

} // namespace media::ffmpeg::graph

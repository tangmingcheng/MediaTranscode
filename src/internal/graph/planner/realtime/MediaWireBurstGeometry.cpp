#include "internal/graph/planner/realtime/MediaWireBurstGeometry.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"

namespace media::ffmpeg::graph {

::media::Result<MediaWireBurstGeometry> MediaWireBurstGeometry::create(
    std::uint64_t udpPayloadBytes,
    std::uint64_t payloadDatagramCount,
    std::uint64_t discreteDatagramCount,
    std::uint64_t maximumUdpPayloadBytes,
    std::uint64_t networkHeaderBytes)
{
    using Result = ::media::Result<MediaWireBurstGeometry>;
    if (udpPayloadBytes == 0 || payloadDatagramCount == 0 ||
        maximumUdpPayloadBytes == 0 || networkHeaderBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire burst geometry requires positive payload, datagram, MTU, and header facts"));
    }
    auto authoritativeDatagramCount = MediaRealtimePlanningArithmetic::add(
        payloadDatagramCount, discreteDatagramCount,
        "wire burst aggregate datagrams");
    if (!authoritativeDatagramCount) {
        return Result::failure(authoritativeDatagramCount.error());
    }
    auto minimumDatagrams = MediaRealtimePlanningArithmetic::ceilScale(
        udpPayloadBytes, 1U, maximumUdpPayloadBytes,
        "wire burst minimum datagrams");
    if (!minimumDatagrams) {
        return Result::failure(minimumDatagrams.error());
    }
    if (authoritativeDatagramCount.value() < minimumDatagrams.value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire burst datagram contract understates payload geometry"));
    }
    auto headerBytes = MediaRealtimePlanningArithmetic::multiply(
        authoritativeDatagramCount.value(), networkHeaderBytes,
        "wire burst network headers");
    auto wireBytes = headerBytes
        ? MediaRealtimePlanningArithmetic::add(
              udpPayloadBytes, headerBytes.value(), "wire burst bytes")
        : headerBytes;
    if (!wireBytes) {
        return Result::failure(wireBytes.error());
    }
    return Result::success(
        {authoritativeDatagramCount.value(), wireBytes.value()});
}

} // namespace media::ffmpeg::graph

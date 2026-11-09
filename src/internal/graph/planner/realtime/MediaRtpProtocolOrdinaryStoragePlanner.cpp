#include "internal/graph/planner/realtime/MediaRtpProtocolOrdinaryStoragePlanner.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/protocol/rtp/MediaRtpNalUnitParser.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaH264RtpDepacketizer.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaHevcRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaAacRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaOpusRtpDepacketizer.h"

namespace media::ffmpeg::graph {
::media::Result<MediaRtpProtocolStorageCosts> MediaRtpProtocolOrdinaryStoragePlanner::plan(
    const MediaPreparedRtpSourceSessionStreamPlan& stream)
{
    using R = ::media::Result<MediaRtpProtocolStorageCosts>;
    const auto packets = stream.workingSet.maximumReorderedPackets;
    const auto datagram = stream.workingSet.maximumDatagramBytes;
    const auto accessUnit = stream.workingSet.maximumAccessUnitBytes;
    if (!packets || !datagram || !accessUnit)
        return R::failure(::media::ErrorInfo::invalidArgument("Protocol allocation geometry requires source bounds"));
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto result = MediaCheckedArithmetic::add(a, b, "Protocol ordinary owners");
        if (!result) { failure = result.error(); return std::uint64_t{0}; }
        return result.value();
    };
    const auto mul = [&](std::uint64_t a, std::uint64_t b) {
        auto result = MediaCheckedArithmetic::multiply(a, b, "Protocol ordinary owners");
        if (!result) { failure = result.error(); return std::uint64_t{0}; }
        return result.value();
    };
    const auto peak = [&](const auto& result) {
        if (!result) { failure = result.error(); return std::uint64_t{0}; }
        return static_cast<std::uint64_t>(result.value().peakBytes);
    };
    auto tree = MediaStandardContainerGeometry::tree<MediaRtpReorderBuffer::StoredValue>(packets);
    auto proxy = MediaStandardContainerGeometry::proxyBytes();
    if (!tree) return R::failure(tree.error());
    if (!proxy) return R::failure(proxy.error());
    std::uint64_t bytes = tree.value();
    std::uint64_t compressed = 0;
    // push's accumulated result and expire's returned result coexist; inserting
    // the second range can retain the old and new first-result backing.
    auto results = add(peak(MediaStandardContainerGeometry::rangeVector<MediaRtpPacket>(packets)),
        peak(MediaStandardContainerGeometry::rangeVector<MediaRtpDiscontinuity>(packets)));
    bytes = add(bytes, mul(2, results));
    // A returned Result has three moved-from vector owners alongside its final
    // optional member. The second result is already included above.
    bytes = add(bytes, mul(6, proxy.value()));

    // Wire CC is four bits. Payload and extension each have exact range/assign
    // allocation; bounding them independently by D preserves per-request costs.
    auto packetBytes = peak(MediaStandardContainerGeometry::vector<std::uint32_t>(15, false));
    const auto packetData = mul(2, peak(MediaStandardContainerGeometry::vector<std::uint8_t>(datagram, false)));
    packetBytes = add(packetBytes, packetData);
    compressed = add(compressed, mul(packets, packetData));
    bytes = add(bytes, mul(packets, packetBytes));
    // Reallocation can contain old/new packet objects, and parser/observe/push/
    // emplace retain moved-from packet shells. Their three vector proxies remain.
    bytes = add(bytes, mul(mul(add(packets, 4), 3), proxy.value()));

    const auto& codec = stream.envelope.codecName;
    if (codec == "h264" || codec == "hevc") {
        bytes = add(bytes, codec == "h264" ? sizeof(MediaH264RtpDepacketizer) : sizeof(MediaHevcRtpDepacketizer));
        // Fragment storage moves into the NAL batch; it is not copied. Assembly
        // is a second independent byte vector. Both use bounded range insertion.
        const auto backing = mul(2, peak(MediaStandardContainerGeometry::rangeVector<std::uint8_t>(accessUnit)));
        bytes = add(bytes, backing);
        compressed = add(compressed, backing);
        bytes = add(bytes, peak(MediaStandardContainerGeometry::rangeVector<MediaRtpNalUnit>(datagram)));
        // Borrowed NALs still contain an empty vector in the debug STL. Account
        // relocation plus the owned-fragment construction/return shells.
        bytes = add(bytes, mul(add(mul(2, datagram), 7), proxy.value()));
    } else if (codec == "aac") {
        bytes = add(bytes, sizeof(MediaAacRtpDepacketizer));
        const auto backing = peak(MediaStandardContainerGeometry::rangeVector<std::uint8_t>(accessUnit));
        bytes = add(bytes, backing);
        compressed = add(compressed, backing);
        // Fragment move into makeRtpAccessUnit keeps a moved-from vector shell.
        bytes = add(bytes, proxy.value());
    } else if (codec == "opus") {
        bytes = add(bytes, sizeof(MediaOpusRtpDepacketizer));
        const auto backing = peak(MediaStandardContainerGeometry::vector<std::uint8_t>(accessUnit, false));
        bytes = add(bytes, backing);
        compressed = add(compressed, backing);
    } else return R::failure(::media::ErrorInfo::unsupported("Protocol ordinary owner layout requires known depacketizer"));
    if (failure) return R::failure(failure);
    if (compressed > bytes) return R::failure(::media::ErrorInfo::invalidArgument("Ordinary protocol classification exceeds total"));
    if (bytes > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Protocol ordinary owner storage overflow"));
    return R::success({static_cast<std::size_t>(bytes - compressed),
        static_cast<std::size_t>(compressed), static_cast<std::size_t>(bytes)});
}
} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaTsPrefixServicePlan.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaRunningTime> datagramDuration(std::uint64_t regular, std::uint64_t last,
    std::size_t count, std::uint64_t overhead, std::uint64_t rate)
{
    using Result = ::media::Result<MediaRunningTime>;
    if (!count || !regular || !last || last > regular)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS service datagram geometry is invalid"));
    const auto duration = [&](std::uint64_t bytes) -> ::media::Result<std::int64_t> {
        auto wire = MediaCheckedArithmetic::add(bytes, overhead, "TS service wire bytes");
        if (!wire) return ::media::Result<std::int64_t>::failure(wire.error());
        return MediaCheckedArithmetic::ceilDurationNanoseconds(wire.value(), rate, "TS service duration");
    };
    auto full = duration(regular);
    auto tail = duration(last);
    if (!full || !tail) return Result::failure(!full ? full.error() : tail.error());
    auto total = MediaCheckedArithmetic::multiply(count - 1, static_cast<std::uint64_t>(full.value()),
        "TS service regular datagrams");
    if (!total) return Result::failure(total.error());
    total = MediaCheckedArithmetic::add(total.value(), static_cast<std::uint64_t>(tail.value()), "TS service tail");
    if (!total) return Result::failure(total.error());
    if (total.value() > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
        return Result::failure(::media::ErrorInfo::invalidArgument("TS service duration overflows"));
    return Result::success(MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(total.value())));
}
}

::media::Result<MediaTsPrefixServicePlan> MediaTsPrefixServicePlan::create(
    const MediaTsDatagramEmissionPlan& emission, const MediaDatagramShapingPlan& shaping,
    std::uint64_t mediaEndpointId, std::optional<MediaTsPrefixReportFacts> report)
{
    using Result = ::media::Result<MediaTsPrefixServicePlan>;
    const auto* endpoint = shaping.endpoint(mediaEndpointId);
    const auto rate = shaping.serviceCurve().maximumWireBytesPerSecond;
    if (!endpoint || !rate || !emission.maximumPayloadBytes() || !emission.packetSizeBytes() ||
        !shaping.batch().maximumDatagrams ||
        shaping.batch().maximumDatagrams > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS prefix service requires activated endpoint and rate facts"));
    if ((emission.perDatagramOverheadBytes() != 0) != report.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS prefix RTP framing and associated report facts disagree"));
    auto maximumDatagram = MediaCheckedArithmetic::add(emission.maximumPayloadBytes(),
        emission.perDatagramOverheadBytes(), "TS prefix maximum datagram bytes");
    if (!maximumDatagram) return Result::failure(maximumDatagram.error());
    if (maximumDatagram.value() > endpoint->maximumDatagramBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS prefix payload exceeds its activated endpoint datagram bound"));
    auto network = MediaCheckedArithmetic::add(endpoint->mtuEvidence.ipHeaderBytes,
        endpoint->mtuEvidence.transportHeaderBytes, "TS prefix network overhead");
    if (!network) return Result::failure(network.error());
    auto overhead = MediaCheckedArithmetic::add(network.value(),
        emission.perDatagramOverheadBytes(), "TS prefix protocol overhead");
    if (!overhead) return Result::failure(overhead.error());
    auto reportDuration = MediaRunningTime::fromNanoseconds(0);
    std::optional<Endpoint> controlEndpoint;
    if (report) {
        const auto* control = shaping.endpoint(report->endpointId);
        auto payload = MediaRtcpWireGeometry::compoundPayloadBytes(report->cnameBytes);
        if (!control || !payload)
            return Result::failure(!payload ? payload.error() :
                ::media::ErrorInfo::invalidArgument("TS prefix RTCP endpoint is absent"));
        if (payload.value() > control->maximumDatagramBytes)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "TS prefix RTCP compound exceeds its endpoint datagram bound"));
        auto headers = MediaCheckedArithmetic::add(control->mtuEvidence.ipHeaderBytes,
            control->mtuEvidence.transportHeaderBytes, "TS prefix RTCP headers");
        if (!headers) return Result::failure(headers.error());
        auto wire = MediaCheckedArithmetic::add(payload.value(), headers.value(), "TS prefix RTCP wire bytes");
        if (!wire) return Result::failure(wire.error());
        auto duration = MediaCheckedArithmetic::ceilDurationNanoseconds(wire.value(), rate,
            "TS prefix associated RTCP allowance");
        if (!duration) return Result::failure(duration.error());
        reportDuration = MediaRunningTime::fromNanoseconds(duration.value());
        controlEndpoint = Endpoint{report->endpointId, headers.value(), control->maximumDatagramBytes};
    }
    return Result::success(MediaTsPrefixServicePlan(rate, emission.maximumPayloadBytes(),
        emission.packetSizeBytes(), overhead.value(), reportDuration,
        Endpoint{mediaEndpointId, network.value(), endpoint->maximumDatagramBytes},
        controlEndpoint, static_cast<std::size_t>(shaping.batch().maximumDatagrams)));
}

::media::Result<MediaRunningTime> MediaTsPrefixServicePlan::durationForPayload(
    std::size_t payloadBytes, std::size_t batches) const
{
    using Result = ::media::Result<MediaRunningTime>;
    if (!payloadBytes || payloadBytes % m_packetBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "TS prefix service payload must contain complete transport packets"));
    const auto full = payloadBytes / m_maximumPayload;
    const auto tail = payloadBytes % m_maximumPayload;
    const auto count = full + (tail != 0);
    if (batches < materializationBatches(count) || batches > count)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS service requires explicit materialization batch bounds"));
    auto media = datagramDuration(m_maximumPayload, tail ? tail : m_maximumPayload,
        count, m_overhead, m_rate);
    if (!media) return Result::failure(media.error());
    auto reports = MediaCheckedArithmetic::multiply(batches, static_cast<std::uint64_t>(m_reportDuration.nanoseconds()),
        "TS future report allowance");
    if (!reports) return Result::failure(reports.error());
    auto total = MediaCheckedArithmetic::add(static_cast<std::uint64_t>(media.value().nanoseconds()), reports.value(),
        "TS prefix media and associated RTCP duration");
    if (!total) return Result::failure(total.error());
    if (total.value() > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
        return Result::failure(::media::ErrorInfo::invalidArgument("TS prefix service duration overflows"));
    return Result::success(MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(total.value())));
}

std::size_t MediaTsPrefixServicePlan::materializationBatches(std::size_t datagrams) const noexcept
{
    return datagrams / m_maximumBatchDatagrams + (datagrams % m_maximumBatchDatagrams != 0);
}

::media::Result<MediaRunningTime> MediaTsPrefixServicePlan::durationForSerialized(
    std::uint64_t endpointId, std::uint64_t regularBytes, std::uint64_t lastBytes,
    std::size_t datagrams) const
{
    const auto* endpoint = endpointId == m_media.id ? &m_media
        : m_control && endpointId == m_control->id ? &*m_control : nullptr;
    if (!endpoint || regularBytes > endpoint->maximumPayload || lastBytes > endpoint->maximumPayload)
        return ::media::Result<MediaRunningTime>::failure(::media::ErrorInfo::invalidArgument(
            "TS serialized service differs from its activated endpoint"));
    return datagramDuration(regularBytes, lastBytes, datagrams, endpoint->networkOverhead, m_rate);
}

} // namespace media::ffmpeg::graph

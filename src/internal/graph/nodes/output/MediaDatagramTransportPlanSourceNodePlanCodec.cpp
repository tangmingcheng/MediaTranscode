#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <charconv>
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner =
    "MediaDatagramTransportPlanSourceNodePlanCodec";
constexpr const char* Prefix = "datagram_transport.";

std::string key(const char* suffix)
{
    return std::string(Prefix) + suffix;
}

std::string producerKey(std::size_t index, const char* suffix)
{
    return std::string(Prefix) + "producer." + std::to_string(index) + "." + suffix;
}

std::string endpointKey(std::size_t index, const char* suffix)
{
    return std::string(Prefix) + "endpoint." + std::to_string(index) + "." +
        suffix;
}

::media::Status set(MediaGraph& graph,
                    MediaNodeId nodeId,
                    std::string option,
                    std::string value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, nodeId, std::move(option), std::move(value));
}

template <typename Value>
::media::Result<Value> parse(const MediaNodeOptions& options,
                            const std::string& option)
{
    auto text = requiredNodeOption(&options, Owner, option.c_str());
    if (!text) return ::media::Result<Value>::failure(text.error());
    std::uint64_t value = 0;
    const char* begin = text.value().data();
    const char* end = begin + text.value().size();
    const auto parsed = std::from_chars(begin, end, value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        value > static_cast<std::uint64_t>((std::numeric_limits<Value>::max)())) {
        return ::media::Result<Value>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Datagram transport node option is not representable"));
    }
    return ::media::Result<Value>::success(static_cast<Value>(value));
}

::media::Result<std::string> required(
    const MediaNodeOptions& options, const std::string& option)
{
    return requiredNodeOption(&options, Owner, option.c_str());
}

} // namespace

::media::Status MediaDatagramTransportPlanSourceNodePlanCodec::apply(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaDatagramTransportPlanTemplate& planTemplate)
{
    if (!graph.findNode(nodeId)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport plan codec requires a planned node"));
    }
    const auto& encoding = planTemplate.encode();
    const auto& d = encoding.deployment;
    const auto put = [&](const char* suffix, const auto& value) {
        return set(graph, nodeId, key(suffix), std::to_string(value));
    };
    for (auto status : {
             set(graph, nodeId, key("session"), encoding.sessionKey),
             put("scope.kind", static_cast<unsigned>(d.serviceScope.kind)),
             set(graph, nodeId, key("scope.id"), d.serviceScope.scopeId),
             set(graph, nodeId, key("scope.authority"), d.serviceScope.coverageAuthority),
             put("mtu.family", static_cast<unsigned>(d.mtu.addressFamily)),
             put("mtu.ip_bytes", d.mtu.maximumIpPacketBytes),
             put("mtu.payload_bytes", d.mtu.senderMaximumPayloadBytes),
             set(graph, nodeId, key("mtu.authority"), d.mtu.authority),
             put("service.capacity_bps",
                 d.service.provisionedCapacityWireBytesPerSecond),
             set(graph, nodeId, key("service.authority"), d.service.authority),
             put("resources.network_bytes", d.resources.maximumNetworkMemoryBytes),
             put("resources.protocol_layout", static_cast<unsigned>(d.resources.protocolLayout)),
             set(graph, nodeId, key("resources.authority"), d.resources.authority),
             put("local.family", static_cast<unsigned>(d.localPorts.addressFamily)),
             set(graph, nodeId, key("local.address"), d.localPorts.numericAddress),
             put("local.first_port", d.localPorts.firstPort),
             put("local.port_count", d.localPorts.portCount),
             set(graph, nodeId, key("local.authority"), d.localPorts.authority),
             put("latency.target_ns", d.latency.targetResidence.nanoseconds()),
             put("latency.maximum_ns", d.latency.maximumResidence.nanoseconds()),
             set(graph, nodeId, key("latency.authority"), d.latency.authority),
             put("latency.release_jitter_ns",
                 d.latency.maximumReleaseJitter.nanoseconds()),
             set(graph, nodeId, key("latency.release_jitter_authority"),
                 d.latency.releaseJitterAuthority),
             put("observation.run_datagrams", d.observation.maximumRunDatagrams),
             put("observation.drain_ns", d.observation.maximumDrainResidence.nanoseconds()),
             put("observation.policy", static_cast<unsigned>(d.observation.evidencePolicy)),
             set(graph, nodeId, key("observation.authority"), d.observation.authority),
             put("transport_timing.sender_lead_ns",
                 d.transportTiming.senderTransportLead.nanoseconds()),
             set(graph, nodeId, key("transport_timing.authority"),
                 d.transportTiming.authority),
             put("rtcp_session.present", d.rtcpSession ? 1 : 0),
             put("rtcp_session.maximum_members", d.rtcpSession
                 ? d.rtcpSession->maximumSessionMembers : 0),
             set(graph, nodeId, key("rtcp_session.authority"), d.rtcpSession
                 ? d.rtcpSession->authority : std::string("none")),
             put("wire.sustained_bps", encoding.wireTraffic.sustainedWireBytesPerSecond),
             put("wire.peak_bps", encoding.wireTraffic.peakWireBytesPerSecond),
             put("wire.peak_datagrams", encoding.wireTraffic.peakDatagramsPerSecond),
             put("wire.burst_bytes", encoding.wireTraffic.burstWireBytes),
             put("wire.burst_datagrams", encoding.wireTraffic.burstDatagrams),
             put("wire.atomic_bytes", encoding.wireTraffic.maximumAtomicWireBytes),
             put("wire.atomic_datagrams", encoding.wireTraffic.maximumAtomicDatagrams),
             put("wire.udp_payload_bytes", encoding.wireTraffic.maximumUdpPayloadBytes),
             put("wire.datagram_bytes", encoding.wireTraffic.maximumWireDatagramBytes),
             set(graph, nodeId, key("wire.authority"), encoding.wireTraffic.authority),
             put("endpoint_count", encoding.remoteEndpoints.size())}) {
        if (!status) return status;
    }
    const auto seriesStorage = d.resources.mpegTsProducer ? d.resources.mpegTsProducer->seriesStorage : std::nullopt;
    for (auto status : {put("mpegts_producer.series.present", seriesStorage ? 1 : 0),
            put("mpegts_producer.series.packets", seriesStorage ? seriesStorage->maximumPackets : 0),
            put("mpegts_producer.series.datagrams", seriesStorage ? seriesStorage->maximumDatagrams : 0),
            put("mpegts_producer.series.live", seriesStorage ? seriesStorage->maximumLiveSeries : 0),
            put("mpegts_producer.series.views", seriesStorage ? seriesStorage->maximumPrefixViews : 0)}) {
        if (!status) return status;
    }
    const auto queueStorage = d.resources.mpegTsProducer ? d.resources.mpegTsProducer->muxQueueStorage : std::nullopt;
    for (auto status : {put("mpegts_producer.queue.present", queueStorage ? 1 : 0),
            put("mpegts_producer.queue.limit", queueStorage ? queueStorage->maximumQueuedBytes : 0)}) {
        if (!status) return status;
    }
    for (auto status : {put("mpegts_producer.present", d.resources.mpegTsProducer ? 1 : 0),
            put("mpegts_producer.datagram_bytes", d.resources.mpegTsProducer ? d.resources.mpegTsProducer->maximumDatagramBytes : 0),
            put("mpegts_producer.rtcp_bytes", d.resources.mpegTsProducer ? d.resources.mpegTsProducer->maximumRtcpCompoundBytes : 0)}) {
        if (!status) return status;
    }
    auto countStatus = put("producer_count", d.resources.rawRtpProducers.size());
    if (!countStatus) return countStatus;
    for (std::size_t index = 0; index < d.resources.rawRtpProducers.size(); ++index) {
        const auto& producer = d.resources.rawRtpProducers[index];
        const auto& emission = producer.emission;
        for (auto status : {
            set(graph, nodeId, producerKey(index, "role"), std::to_string(static_cast<unsigned>(producer.role))),
            set(graph, nodeId, producerKey(index, "mode"), std::to_string(static_cast<unsigned>(emission.packetizationMode()))),
            set(graph, nodeId, producerKey(index, "au_bytes"), std::to_string(emission.maximumAccessUnitPayloadBytes())),
            set(graph, nodeId, producerKey(index, "datagram_bytes"), std::to_string(emission.maximumDatagramBytes())),
            set(graph, nodeId, producerKey(index, "au_datagrams"), std::to_string(emission.maximumDatagramsPerAccessUnit())),
            set(graph, nodeId, producerKey(index, "authority"), emission.authority()),
            set(graph, nodeId, producerKey(index, "layout"), emission.packetLayout() ? "start-code" : "none"),
            set(graph, nodeId, producerKey(index, "rtcp_bytes"), std::to_string(producer.maximumRtcpCompoundBytes))}) {
            if (!status) return status;
        }
    }
    for (std::size_t index = 0; index < encoding.remoteEndpoints.size(); ++index) {
        const auto& endpoint = encoding.remoteEndpoints[index];
        for (auto status : {
                 set(graph, nodeId, endpointKey(index, "id"),
                     std::to_string(endpoint.endpointId)),
                 set(graph, nodeId, endpointKey(index, "role"),
                     std::to_string(static_cast<unsigned>(endpoint.role))),
                 set(graph, nodeId, endpointKey(index, "family"),
                     std::to_string(static_cast<unsigned>(endpoint.addressFamily))),
                 set(graph, nodeId, endpointKey(index, "address"),
                     endpoint.numericAddress),
                 set(graph, nodeId, endpointKey(index, "port"),
                     std::to_string(endpoint.port))}) {
            if (!status) return status;
        }
    }
    return ::media::Status::success();
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaDatagramTransportPlanSourceNodePlanCodec::decode(const MediaNode& node)
{
    using Result = ::media::Result<MediaDatagramTransportPlanTemplate>;
    auto session = required(node.options, key("session"));
    auto scopeKind = parse<std::uint8_t>(node.options, key("scope.kind"));
    auto scopeId = required(node.options, key("scope.id"));
    auto scopeAuthority = required(node.options, key("scope.authority"));
    auto mtuFamily = parse<std::uint8_t>(node.options, key("mtu.family"));
    auto mtuIp = parse<std::uint64_t>(node.options, key("mtu.ip_bytes"));
    auto mtuPayload = parse<std::uint64_t>(node.options, key("mtu.payload_bytes"));
    auto mtuAuthority = required(node.options, key("mtu.authority"));
    auto provisionedCapacity = parse<std::uint64_t>(
        node.options, key("service.capacity_bps"));
    auto serviceAuthority = required(node.options, key("service.authority"));
    auto networkBytes = parse<std::uint64_t>(node.options, key("resources.network_bytes"));
    auto resourcesAuthority = required(node.options, key("resources.authority"));
    auto protocolLayout = parse<std::uint8_t>(node.options, key("resources.protocol_layout"));
    auto localFamily = parse<std::uint8_t>(node.options, key("local.family"));
    auto localAddress = required(node.options, key("local.address"));
    auto firstPort = parse<std::uint16_t>(node.options, key("local.first_port"));
    auto portCount = parse<std::uint16_t>(node.options, key("local.port_count"));
    auto localAuthority = required(node.options, key("local.authority"));
    auto targetLatency = parse<std::int64_t>(node.options, key("latency.target_ns"));
    auto maximumLatency = parse<std::int64_t>(node.options, key("latency.maximum_ns"));
    auto latencyAuthority = required(node.options, key("latency.authority"));
    auto releaseJitter = parse<std::int64_t>(
        node.options, key("latency.release_jitter_ns"));
    auto releaseJitterAuthority = required(
        node.options, key("latency.release_jitter_authority"));
    auto runDatagrams = parse<std::uint64_t>(node.options, key("observation.run_datagrams"));
    auto drain = parse<std::int64_t>(node.options, key("observation.drain_ns"));
    auto evidencePolicy = parse<std::uint8_t>(node.options, key("observation.policy"));
    auto observationAuthority = required(node.options, key("observation.authority"));
    auto senderTransportLead = parse<std::int64_t>(
        node.options, key("transport_timing.sender_lead_ns"));
    auto transportTimingAuthority = required(
        node.options, key("transport_timing.authority"));
    auto rtcpSessionPresent = parse<std::uint8_t>(
        node.options, key("rtcp_session.present"));
    auto rtcpSessionMembers = parse<std::uint32_t>(
        node.options, key("rtcp_session.maximum_members"));
    auto rtcpSessionAuthority = required(
        node.options, key("rtcp_session.authority"));
    auto wireSustained = parse<std::uint64_t>(node.options, key("wire.sustained_bps"));
    auto wirePeak = parse<std::uint64_t>(node.options, key("wire.peak_bps"));
    auto wirePackets = parse<std::uint64_t>(node.options, key("wire.peak_datagrams"));
    auto wireBurst = parse<std::uint64_t>(node.options, key("wire.burst_bytes"));
    auto wireBurstDatagrams = parse<std::uint64_t>(
        node.options, key("wire.burst_datagrams"));
    auto wireAtomicBytes = parse<std::uint64_t>(
        node.options, key("wire.atomic_bytes"));
    auto wireAtomicDatagrams = parse<std::uint64_t>(
        node.options, key("wire.atomic_datagrams"));
    auto wirePayload = parse<std::uint64_t>(node.options, key("wire.udp_payload_bytes"));
    auto wireDatagram = parse<std::uint64_t>(node.options, key("wire.datagram_bytes"));
    auto wireAuthority = required(node.options, key("wire.authority"));
    auto endpointCount = parse<std::size_t>(node.options, key("endpoint_count"));
#define REQUIRE_VALUE(name) if (!(name)) return Result::failure((name).error())
    REQUIRE_VALUE(session); REQUIRE_VALUE(scopeKind); REQUIRE_VALUE(scopeId);
    REQUIRE_VALUE(scopeAuthority); REQUIRE_VALUE(mtuFamily); REQUIRE_VALUE(mtuIp);
    REQUIRE_VALUE(mtuPayload); REQUIRE_VALUE(mtuAuthority);
    REQUIRE_VALUE(provisionedCapacity);
    REQUIRE_VALUE(serviceAuthority);
    REQUIRE_VALUE(networkBytes); REQUIRE_VALUE(protocolLayout);
    REQUIRE_VALUE(resourcesAuthority); REQUIRE_VALUE(localFamily);
    REQUIRE_VALUE(localAddress); REQUIRE_VALUE(firstPort); REQUIRE_VALUE(portCount);
    REQUIRE_VALUE(localAuthority); REQUIRE_VALUE(targetLatency); REQUIRE_VALUE(maximumLatency);
    REQUIRE_VALUE(latencyAuthority); REQUIRE_VALUE(releaseJitter);
    REQUIRE_VALUE(releaseJitterAuthority); REQUIRE_VALUE(runDatagrams);
    REQUIRE_VALUE(drain); REQUIRE_VALUE(evidencePolicy); REQUIRE_VALUE(observationAuthority);
    REQUIRE_VALUE(senderTransportLead); REQUIRE_VALUE(transportTimingAuthority);
    REQUIRE_VALUE(rtcpSessionPresent); REQUIRE_VALUE(rtcpSessionMembers);
    REQUIRE_VALUE(rtcpSessionAuthority);
    REQUIRE_VALUE(wireSustained); REQUIRE_VALUE(wirePeak); REQUIRE_VALUE(wirePackets);
    REQUIRE_VALUE(wireBurst); REQUIRE_VALUE(wireBurstDatagrams);
    REQUIRE_VALUE(wireAtomicBytes); REQUIRE_VALUE(wireAtomicDatagrams);
    REQUIRE_VALUE(wirePayload); REQUIRE_VALUE(wireDatagram);
    REQUIRE_VALUE(wireAuthority);
    REQUIRE_VALUE(endpointCount);
#undef REQUIRE_VALUE
    auto producerCount = parse<std::size_t>(node.options, key("producer_count"));
    if (!producerCount) return Result::failure(producerCount.error());
    const auto layout = static_cast<MediaDatagramProtocolLayout>(protocolLayout.value());
    auto topology = mediaDatagramProtocolTopology(layout);
    if (!topology) return Result::failure(topology.error());
    const bool raw = layout == MediaDatagramProtocolLayout::VideoRtp ||
        layout == MediaDatagramProtocolLayout::SeparateAvRtp;
    if (producerCount.value() != (raw ? topology.value().rtpInstances.size() : 0))
        return Result::failure(::media::ErrorInfo::invalidArgument("Raw RTP producer option count differs from topology"));
    auto queuePresent = parse<unsigned>(node.options, key("mpegts_producer.queue.present"));
    auto queueLimit = parse<std::uint64_t>(node.options, key("mpegts_producer.queue.limit"));
    if (!queuePresent || !queueLimit) return Result::failure(!queuePresent ? queuePresent.error() : queueLimit.error());
    if (queuePresent.value() > 1 || (!queuePresent.value() && queueLimit.value()))
        return Result::failure(::media::ErrorInfo::invalidArgument("TS mux queue encoding is inconsistent"));
    std::optional<MediaTsMuxQueueStoragePlan> queueStorage;
    if (queuePresent.value()) {
        auto planned = MediaTsMuxQueueStoragePlanner::plan(queueLimit.value());
        if (!planned) return Result::failure(planned.error());
        queueStorage = planned.value();
    }
    auto seriesPresent = parse<unsigned>(node.options, key("mpegts_producer.series.present"));
    auto seriesPackets = parse<std::size_t>(node.options, key("mpegts_producer.series.packets"));
    auto seriesDatagrams = parse<std::size_t>(node.options, key("mpegts_producer.series.datagrams"));
    auto seriesLive = parse<std::size_t>(node.options, key("mpegts_producer.series.live"));
    auto seriesViews = parse<std::size_t>(node.options, key("mpegts_producer.series.views"));
    if (!seriesPresent || !seriesPackets || !seriesDatagrams || !seriesLive || !seriesViews)
        return Result::failure(!seriesPresent ? seriesPresent.error() : !seriesPackets ? seriesPackets.error()
            : !seriesDatagrams ? seriesDatagrams.error() : !seriesLive ? seriesLive.error() : seriesViews.error());
    if (seriesPresent.value() > 1 || (!seriesPresent.value() &&
        (seriesPackets.value() || seriesDatagrams.value() || seriesLive.value() || seriesViews.value())))
        return Result::failure(::media::ErrorInfo::invalidArgument("TS series storage encoding is inconsistent"));
    std::optional<MediaTsSeriesStoragePlan> seriesStorage;
    if (seriesPresent.value()) {
        auto planned = MediaTsSeriesStoragePlanner::plan(seriesPackets.value(), seriesDatagrams.value(), seriesLive.value(), seriesViews.value());
        if (!planned) return Result::failure(planned.error());
        seriesStorage = planned.value();
    }
    auto mpegTsPresent = parse<unsigned>(node.options, key("mpegts_producer.present"));
    auto mpegTsBytes = parse<std::uint64_t>(node.options, key("mpegts_producer.datagram_bytes"));
    auto mpegTsRtcp = parse<std::uint64_t>(node.options, key("mpegts_producer.rtcp_bytes"));
    if (!mpegTsPresent || !mpegTsBytes || !mpegTsRtcp)
        return Result::failure(!mpegTsPresent ? mpegTsPresent.error() : !mpegTsBytes ? mpegTsBytes.error() : mpegTsRtcp.error());
    if (mpegTsPresent.value() > 1 || (!mpegTsPresent.value() && (mpegTsBytes.value() || mpegTsRtcp.value())))
        return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS producer encoding is inconsistent"));
    std::optional<MediaMpegTsProducerPlan> mpegTsProducer;
    if (mpegTsPresent.value()) mpegTsProducer.emplace(MediaMpegTsProducerPlan{mpegTsBytes.value(), mpegTsRtcp.value(), queueStorage, seriesStorage});
    if ((queueStorage || seriesStorage) && !mpegTsProducer) return Result::failure(
        ::media::ErrorInfo::invalidArgument("Non-TS producer carries TS queue storage"));
    auto mpegTsValid = validateMpegTsProducerPlan(layout, mpegTsProducer);
    if (!mpegTsValid) return Result::failure(mpegTsValid.error());
    std::vector<MediaRawRtpProducerPlan> producers;
    try { producers.reserve(producerCount.value()); }
    catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Raw RTP producer plan options"));
    }
    for (std::size_t index = 0; index < producerCount.value(); ++index) {
        auto role = parse<std::uint8_t>(node.options, producerKey(index, "role"));
        auto mode = parse<std::uint8_t>(node.options, producerKey(index, "mode"));
        auto au = parse<std::uint64_t>(node.options, producerKey(index, "au_bytes"));
        auto datagram = parse<std::size_t>(node.options, producerKey(index, "datagram_bytes"));
        auto count = parse<std::uint64_t>(node.options, producerKey(index, "au_datagrams"));
        auto authority = required(node.options, producerKey(index, "authority"));
        auto packetLayout = required(node.options, producerKey(index, "layout"));
        auto rtcp = parse<std::uint64_t>(node.options, producerKey(index, "rtcp_bytes"));
        if (!role || !mode || !au || !datagram || !count || !authority || !packetLayout || !rtcp)
            return Result::failure(!role ? role.error() : !mode ? mode.error() : !au ? au.error() :
                !datagram ? datagram.error() : !count ? count.error() : !authority ? authority.error() :
                !packetLayout ? packetLayout.error() : rtcp.error());
        const auto selected = static_cast<MediaScheduledRtpPacketizationMode>(mode.value());
        const bool audio = selected == MediaScheduledRtpPacketizationMode::AacLatm;
        if (packetLayout.value() != (audio ? "none" : "start-code"))
            return Result::failure(::media::ErrorInfo::invalidArgument("Raw RTP producer packet layout is inconsistent"));
        auto contract = audio
            ? MediaRtpAccessUnitEmissionContract::createAacLatm(au.value(), datagram.value(), std::move(authority).value())
            : MediaRtpAccessUnitEmissionContract::createVideo(selected, MediaEncodedPacketLayout::startCodeDelimited(),
                au.value(), datagram.value(), std::move(authority).value());
        if (!contract) return Result::failure(contract.error());
        if (contract.value().maximumDatagramsPerAccessUnit() != count.value())
            return Result::failure(::media::ErrorInfo::invalidArgument("Raw RTP producer packet count differs from emission contract"));
        producers.push_back({static_cast<MediaDatagramProtocolEndpointRole>(role.value()),
            std::move(contract).value(), rtcp.value()});
    }
    MediaRealtimeDeploymentEnvelopeEncoding deployment{
        {static_cast<MediaDatagramServiceScopeKind>(scopeKind.value()),
         std::move(scopeId).value(), std::move(scopeAuthority).value()},
        {static_cast<MediaIpAddressFamily>(mtuFamily.value()),
         std::move(mtuAuthority).value(), mtuIp.value(), mtuPayload.value()},
        {provisionedCapacity.value(),
         std::move(serviceAuthority).value()},
        {networkBytes.value(), static_cast<MediaDatagramProtocolLayout>(protocolLayout.value()),
         std::move(resourcesAuthority).value(), std::move(producers), std::move(mpegTsProducer)},
        {static_cast<MediaIpAddressFamily>(localFamily.value()),
         std::move(localAddress).value(), firstPort.value(), portCount.value(),
         std::move(localAuthority).value()},
        {MediaRunningTime::fromNanoseconds(targetLatency.value()),
         MediaRunningTime::fromNanoseconds(maximumLatency.value()),
         std::move(latencyAuthority).value(),
         MediaRunningTime::fromNanoseconds(releaseJitter.value()),
         std::move(releaseJitterAuthority).value()},
        {runDatagrams.value(),
         MediaRunningTime::fromNanoseconds(drain.value()),
         static_cast<MediaRealtimeTransmitEvidencePolicy>(evidencePolicy.value()),
         std::move(observationAuthority).value()},
        {MediaRunningTime::fromNanoseconds(senderTransportLead.value()),
         std::move(transportTimingAuthority).value()},
        std::nullopt};
    if (rtcpSessionPresent.value() == 1) {
        deployment.rtcpSession = MediaRealtimeRtcpSessionCapability{
            rtcpSessionMembers.value(),
            std::move(rtcpSessionAuthority).value()};
    } else if (rtcpSessionPresent.value() != 0 ||
               rtcpSessionMembers.value() != 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTCP session capability presence is invalid"));
    }
    auto decodedDeployment = MediaRealtimeDeploymentEnvelope::decode(
        std::move(deployment));
    if (!decodedDeployment) return Result::failure(decodedDeployment.error());
    std::vector<MediaDatagramRemoteEndpointFact> endpoints;
    try { endpoints.reserve(endpointCount.value()); }
    catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Datagram transport endpoint plan"));
    }
    for (std::size_t index = 0; index < endpointCount.value(); ++index) {
        auto id = parse<std::uint64_t>(node.options, endpointKey(index, "id"));
        auto role = parse<std::uint8_t>(node.options, endpointKey(index, "role"));
        auto family = parse<std::uint8_t>(node.options, endpointKey(index, "family"));
        auto address = required(node.options, endpointKey(index, "address"));
        auto port = parse<std::uint16_t>(node.options, endpointKey(index, "port"));
        if (!id || !role || !family || !address || !port) {
            return Result::failure(!id ? id.error() : !role ? role.error() :
                !family ? family.error() : !address ? address.error() : port.error());
        }
        endpoints.push_back({id.value(),
            static_cast<MediaDatagramProtocolEndpointRole>(role.value()),
            static_cast<MediaIpAddressFamily>(family.value()),
            std::move(address).value(), port.value()});
    }
    return MediaDatagramTransportPlanTemplate::create(
        std::move(session).value(), decodedDeployment.value(),
        std::move(endpoints),
        MediaWireTrafficEnvelope{
            wireSustained.value(), wirePeak.value(), wirePackets.value(),
            wireBurst.value(), wireBurstDatagrams.value(),
            wireAtomicBytes.value(), wireAtomicDatagrams.value(),
            wirePayload.value(), wireDatagram.value(),
            std::move(wireAuthority).value()});
}

} // namespace media::ffmpeg::graph

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
             put("service.sustained_bps", d.service.sustainedWireBytesPerSecond),
             put("service.peak_bps", d.service.peakWireBytesPerSecond),
             put("service.burst_bytes", d.service.burstWireBytes),
             set(graph, nodeId, key("service.authority"), d.service.authority),
             put("resources.graph_bytes", d.resources.maximumGraphMemoryBytes),
             put("resources.network_bytes", d.resources.maximumNetworkMemoryBytes),
             put("resources.socket_bytes", d.resources.maximumSocketMemoryBytes),
             set(graph, nodeId, key("resources.authority"), d.resources.authority),
             put("local.family", static_cast<unsigned>(d.localPorts.addressFamily)),
             set(graph, nodeId, key("local.address"), d.localPorts.numericAddress),
             put("local.first_port", d.localPorts.firstPort),
             put("local.port_count", d.localPorts.portCount),
             set(graph, nodeId, key("local.authority"), d.localPorts.authority),
             put("latency.target_ns", d.latency.targetResidence.nanoseconds()),
             put("latency.maximum_ns", d.latency.maximumResidence.nanoseconds()),
             set(graph, nodeId, key("latency.authority"), d.latency.authority),
             put("observation.run_datagrams", d.observation.maximumRunDatagrams),
             put("observation.drain_ns", d.observation.maximumDrainResidence.nanoseconds()),
             put("observation.policy", static_cast<unsigned>(d.observation.evidencePolicy)),
             set(graph, nodeId, key("observation.authority"), d.observation.authority),
             put("receiver.present", d.receiverTiming ? 1 : 0),
             put("receiver.decode_lead_ns", d.receiverTiming
                 ? d.receiverTiming->transportDecodeLead.nanoseconds() : 0),
             set(graph, nodeId, key("receiver.authority"), d.receiverTiming
                 ? d.receiverTiming->authority : std::string("none")),
             put("wire.sustained_bps", encoding.wireTraffic.sustainedWireBytesPerSecond),
             put("wire.peak_bps", encoding.wireTraffic.peakWireBytesPerSecond),
             put("wire.peak_datagrams", encoding.wireTraffic.peakDatagramsPerSecond),
             put("wire.burst_bytes", encoding.wireTraffic.burstWireBytes),
             put("wire.udp_payload_bytes", encoding.wireTraffic.maximumUdpPayloadBytes),
             put("wire.datagram_bytes", encoding.wireTraffic.maximumWireDatagramBytes),
             set(graph, nodeId, key("wire.authority"), encoding.wireTraffic.authority),
             put("endpoint_count", encoding.remoteEndpoints.size())}) {
        if (!status) return status;
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
    auto sustained = parse<std::uint64_t>(node.options, key("service.sustained_bps"));
    auto peak = parse<std::uint64_t>(node.options, key("service.peak_bps"));
    auto burst = parse<std::uint64_t>(node.options, key("service.burst_bytes"));
    auto serviceAuthority = required(node.options, key("service.authority"));
    auto graphBytes = parse<std::uint64_t>(node.options, key("resources.graph_bytes"));
    auto networkBytes = parse<std::uint64_t>(node.options, key("resources.network_bytes"));
    auto socketBytes = parse<std::uint64_t>(node.options, key("resources.socket_bytes"));
    auto resourcesAuthority = required(node.options, key("resources.authority"));
    auto localFamily = parse<std::uint8_t>(node.options, key("local.family"));
    auto localAddress = required(node.options, key("local.address"));
    auto firstPort = parse<std::uint16_t>(node.options, key("local.first_port"));
    auto portCount = parse<std::uint16_t>(node.options, key("local.port_count"));
    auto localAuthority = required(node.options, key("local.authority"));
    auto targetLatency = parse<std::int64_t>(node.options, key("latency.target_ns"));
    auto maximumLatency = parse<std::int64_t>(node.options, key("latency.maximum_ns"));
    auto latencyAuthority = required(node.options, key("latency.authority"));
    auto runDatagrams = parse<std::uint64_t>(node.options, key("observation.run_datagrams"));
    auto drain = parse<std::int64_t>(node.options, key("observation.drain_ns"));
    auto evidencePolicy = parse<std::uint8_t>(node.options, key("observation.policy"));
    auto observationAuthority = required(node.options, key("observation.authority"));
    auto receiverPresent = parse<std::uint8_t>(node.options, key("receiver.present"));
    auto receiverDecodeLead = parse<std::int64_t>(node.options, key("receiver.decode_lead_ns"));
    auto receiverAuthority = required(node.options, key("receiver.authority"));
    auto wireSustained = parse<std::uint64_t>(node.options, key("wire.sustained_bps"));
    auto wirePeak = parse<std::uint64_t>(node.options, key("wire.peak_bps"));
    auto wirePackets = parse<std::uint64_t>(node.options, key("wire.peak_datagrams"));
    auto wireBurst = parse<std::uint64_t>(node.options, key("wire.burst_bytes"));
    auto wirePayload = parse<std::uint64_t>(node.options, key("wire.udp_payload_bytes"));
    auto wireDatagram = parse<std::uint64_t>(node.options, key("wire.datagram_bytes"));
    auto wireAuthority = required(node.options, key("wire.authority"));
    auto endpointCount = parse<std::size_t>(node.options, key("endpoint_count"));
#define REQUIRE_VALUE(name) if (!(name)) return Result::failure((name).error())
    REQUIRE_VALUE(session); REQUIRE_VALUE(scopeKind); REQUIRE_VALUE(scopeId);
    REQUIRE_VALUE(scopeAuthority); REQUIRE_VALUE(mtuFamily); REQUIRE_VALUE(mtuIp);
    REQUIRE_VALUE(mtuPayload); REQUIRE_VALUE(mtuAuthority);
    REQUIRE_VALUE(sustained); REQUIRE_VALUE(peak); REQUIRE_VALUE(burst);
    REQUIRE_VALUE(serviceAuthority); REQUIRE_VALUE(graphBytes);
    REQUIRE_VALUE(networkBytes);
    REQUIRE_VALUE(socketBytes); REQUIRE_VALUE(resourcesAuthority); REQUIRE_VALUE(localFamily);
    REQUIRE_VALUE(localAddress); REQUIRE_VALUE(firstPort); REQUIRE_VALUE(portCount);
    REQUIRE_VALUE(localAuthority); REQUIRE_VALUE(targetLatency); REQUIRE_VALUE(maximumLatency);
    REQUIRE_VALUE(latencyAuthority); REQUIRE_VALUE(runDatagrams);
    REQUIRE_VALUE(drain); REQUIRE_VALUE(evidencePolicy); REQUIRE_VALUE(observationAuthority);
    REQUIRE_VALUE(receiverPresent); REQUIRE_VALUE(receiverDecodeLead);
    REQUIRE_VALUE(receiverAuthority);
    REQUIRE_VALUE(wireSustained); REQUIRE_VALUE(wirePeak); REQUIRE_VALUE(wirePackets);
    REQUIRE_VALUE(wireBurst); REQUIRE_VALUE(wirePayload); REQUIRE_VALUE(wireDatagram);
    REQUIRE_VALUE(wireAuthority);
    REQUIRE_VALUE(endpointCount);
#undef REQUIRE_VALUE
    MediaRealtimeDeploymentEnvelopeEncoding deployment{
        {static_cast<MediaDatagramServiceScopeKind>(scopeKind.value()),
         std::move(scopeId).value(), std::move(scopeAuthority).value()},
        {static_cast<MediaIpAddressFamily>(mtuFamily.value()),
         std::move(mtuAuthority).value(), mtuIp.value(), mtuPayload.value()},
        {sustained.value(), peak.value(), burst.value(),
         std::move(serviceAuthority).value()},
        {graphBytes.value(), networkBytes.value(), socketBytes.value(),
         std::move(resourcesAuthority).value()},
        {static_cast<MediaIpAddressFamily>(localFamily.value()),
         std::move(localAddress).value(), firstPort.value(), portCount.value(),
         std::move(localAuthority).value()},
        {MediaRunningTime::fromNanoseconds(targetLatency.value()),
         MediaRunningTime::fromNanoseconds(maximumLatency.value()),
         std::move(latencyAuthority).value()},
        {runDatagrams.value(),
         MediaRunningTime::fromNanoseconds(drain.value()),
         static_cast<MediaRealtimeTransmitEvidencePolicy>(evidencePolicy.value()),
         std::move(observationAuthority).value()},
        std::nullopt};
    if (receiverPresent.value() == 1) {
        deployment.receiverTiming = MediaRealtimeReceiverTimingCapability{
            MediaRunningTime::fromNanoseconds(receiverDecodeLead.value()),
            std::move(receiverAuthority).value()};
    } else if (receiverPresent.value() != 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "receiver timing presence flag is invalid"));
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
            wireBurst.value(), wirePayload.value(), wireDatagram.value(),
            std::move(wireAuthority).value()});
}

} // namespace media::ffmpeg::graph

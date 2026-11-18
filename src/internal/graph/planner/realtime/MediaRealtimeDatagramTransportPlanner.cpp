#include "internal/graph/planner/realtime/MediaRealtimeDatagramTransportPlanner.h"

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/planner/realtime/MediaWireTrafficEnvelopePlanner.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"

#include <new>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Status validatePreparedProducer(
    const MediaRealtimeDeploymentEnvelope& deployment,
    MediaDatagramProtocolEndpointRole role,
    const MediaScheduledRtpOutputPlan& output)
{
    auto compound = MediaRtcpWireGeometry::compoundPayloadBytes(output.cname.size());
    if (!compound) return ::media::Status::failure(compound.error());
    for (const auto& producer : deployment.encode().resources.rawRtpProducers) {
        if (producer.role != role) continue;
        if (producer.emission == output.packetization.emissionContract() &&
            producer.maximumRtcpCompoundBytes == compound.value())
            return ::media::Status::success();
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP output differs from deployment producer facts"));
    }
    return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "Scheduled RTP output has no deployment producer facts"));
}

void appendRtpEndpoints(
    std::vector<MediaDatagramRemoteEndpointFact>& endpoints,
    const MediaRtpRemoteEndpointPair& transport,
    MediaDatagramProtocolEndpointRole rtpRole,
    MediaDatagramProtocolEndpointRole rtcpRole)
{
    const auto& rtp = transport.remoteRtpEndpoint();
    const auto& rtcp = transport.remoteRtcpEndpoint();
    const auto nextId = static_cast<std::uint64_t>(endpoints.size()) + 1U;
    endpoints.push_back({nextId, rtpRole, rtp.addressFamily(),
                         rtp.numericAddress(), rtp.port()});
    endpoints.push_back({nextId + 1U, rtcpRole, rtcp.addressFamily(),
                         rtcp.numericAddress(), rtcp.port()});
}

::media::Result<MediaDatagramTransportPlanTemplate> create(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    std::vector<MediaDatagramRemoteEndpointFact> endpoints,
    ::media::Result<MediaWireTrafficEnvelope> wire)
{
    if (!wire) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            wire.error());
    }
    return MediaDatagramTransportPlanTemplate::create(
        sessionKey, deployment, std::move(endpoints),
        std::move(wire).value());
}

} // namespace

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaVideoOnlySeparateRtpOutputRuntimePlan& output,
    const MediaPreparedRealtimeEmissionSet& emission)
{
    auto video = validatePreparedProducer(deployment,
        MediaDatagramProtocolEndpointRole::VideoRtp, output.video);
    if (!video) return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(video.error());
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints;
        endpoints.reserve(2);
        appendRtpEndpoints(
            endpoints, output.video.transport,
            MediaDatagramProtocolEndpointRole::VideoRtp,
            MediaDatagramProtocolEndpointRole::VideoRtcp);
        return create(
            sessionKey, deployment, std::move(endpoints),
            MediaWireTrafficEnvelopePlanner::plan(
                deployment, emission, output));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            ::media::ErrorInfo::allocationFailed(
                "VideoOnly RTP Datagram transport planning"));
    }
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaSeparateRtpOutputRuntimePlan& output,
    const MediaPreparedRealtimeEmissionSet& emission)
{
    auto video = validatePreparedProducer(deployment,
        MediaDatagramProtocolEndpointRole::VideoRtp, output.video);
    if (!video) return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(video.error());
    auto audio = validatePreparedProducer(deployment,
        MediaDatagramProtocolEndpointRole::AudioRtp, output.audio);
    if (!audio) return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(audio.error());
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints;
        endpoints.reserve(4);
        appendRtpEndpoints(
            endpoints, output.video.transport,
            MediaDatagramProtocolEndpointRole::VideoRtp,
            MediaDatagramProtocolEndpointRole::VideoRtcp);
        appendRtpEndpoints(
            endpoints, output.audio.transport,
            MediaDatagramProtocolEndpointRole::AudioRtp,
            MediaDatagramProtocolEndpointRole::AudioRtcp);
        return create(
            sessionKey, deployment, std::move(endpoints),
            MediaWireTrafficEnvelopePlanner::plan(
                deployment, emission, output));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            ::media::ErrorInfo::allocationFailed(
                "A/V RTP Datagram transport planning"));
    }
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaProjectMpegTsRuntimeOutputPlan& output,
    const MediaPreparedRealtimeEmissionSet& emission)
{
    const auto& params = output.protocol.muxPlan().parameters();
    const auto* rtpFacts = std::get_if<MediaMpegTsRtpOutputPlan>(&output.transport);
    auto compound = rtpFacts
        ? MediaRtcpWireGeometry::compoundPayloadBytes(rtpFacts->cname().size())
        : ::media::Result<std::uint64_t>::success(0);
    if (!compound) return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(compound.error());
    const MediaMpegTsProducerPlan actual{
        static_cast<std::uint64_t>(params.maximumPacketsPerDatagram) * params.packetSize + (rtpFacts ? 12 : 0),
        compound.value()};
    const auto& planned = deployment.encode().resources.mpegTsProducer;
    if (!planned || *planned != actual || (rtpFacts && rtpFacts->maximumDatagramBytes() != actual.maximumDatagramBytes))
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS output differs from deployment producer facts"));
    auto wire = MediaWireTrafficEnvelopePlanner::plan(
        deployment, emission, output);
    if (!wire) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            wire.error());
    }
    if (const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
            &output.transport)) {
        try {
            std::vector<MediaDatagramRemoteEndpointFact> endpoints;
            endpoints.reserve(2);
            appendRtpEndpoints(
                endpoints, rtp->transport(),
                MediaDatagramProtocolEndpointRole::MpegTsRtp,
                MediaDatagramProtocolEndpointRole::MpegTsRtcp);
            return create(sessionKey, deployment, std::move(endpoints),
                          std::move(wire));
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "MPEG-TS RTP Datagram transport planning"));
        }
    }
    const auto* udp = std::get_if<MediaMpegTsUdpOutputPlan>(
        &output.transport);
    auto parsed = udp
        ? parseRtpUdpUrlEndpoint(udp->url)
        : ::media::Result<MediaRtpUrlEndpoint>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "MPEG-TS transport variant is unavailable"));
    if (!parsed || parsed.value().scheme != "udp") {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            parsed ? ::media::ErrorInfo::invalidArgument(
                         "MPEG-TS Datagram transport requires udp://")
                   : parsed.error());
    }
    MediaIpAddressFamily family = MediaIpAddressFamily::Ipv4;
    if (!MediaNumericIpAddress::create(family, parsed.value().host)) {
        family = MediaIpAddressFamily::Ipv6;
        if (!MediaNumericIpAddress::create(family, parsed.value().host)) {
            return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS UDP destination must be a numeric address"));
        }
    }
    return create(
        sessionKey, deployment,
        {{1, MediaDatagramProtocolEndpointRole::MpegTsUdp,
          family, parsed.value().host, parsed.value().port}},
        std::move(wire));
}

} // namespace media::ffmpeg::graph

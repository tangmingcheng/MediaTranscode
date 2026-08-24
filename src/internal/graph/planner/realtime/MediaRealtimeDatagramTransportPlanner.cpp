#include "internal/graph/planner/realtime/MediaRealtimeDatagramTransportPlanner.h"

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <utility>
#include <variant>
#include <new>

namespace media::ffmpeg::graph {
namespace {

void appendRtpEndpoints(
    std::vector<MediaDatagramRemoteEndpointFact>& endpoints,
    const MediaRtpUdpSenderConfig& transport)
{
    const auto& rtp = transport.remoteRtpEndpoint();
    const auto& rtcp = transport.remoteRtcpEndpoint();
    const auto nextId = static_cast<std::uint64_t>(endpoints.size()) + 1U;
    endpoints.push_back({nextId, rtp.addressFamily(),
                         rtp.numericAddress(), rtp.port()});
    endpoints.push_back({nextId + 1U, rtcp.addressFamily(),
                         rtcp.numericAddress(), rtcp.port()});
}

::media::Result<MediaDatagramTransportPlanTemplate> planRtp(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    std::vector<MediaDatagramRemoteEndpointFact> endpoints)
{
    return MediaDatagramTransportPlanTemplate::create(
        sessionKey, deployment, std::move(endpoints));
}

} // namespace

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaVideoOnlySeparateRtpOutputRuntimePlan& output)
{
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints;
        endpoints.reserve(2);
        appendRtpEndpoints(endpoints, output.video.transport);
        return planRtp(sessionKey, deployment, std::move(endpoints));
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
    const MediaSeparateRtpOutputRuntimePlan& output)
{
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints;
        endpoints.reserve(4);
        appendRtpEndpoints(endpoints, output.video.transport);
        appendRtpEndpoints(endpoints, output.audio.transport);
        return planRtp(sessionKey, deployment, std::move(endpoints));
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
    const MediaProjectMpegTsRuntimeOutputPlan& output)
{
    if (const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
            &output.transport)) {
        try {
            std::vector<MediaDatagramRemoteEndpointFact> endpoints;
            endpoints.reserve(2);
            appendRtpEndpoints(endpoints, rtp->transport());
            return planRtp(sessionKey, deployment, std::move(endpoints));
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
    return MediaDatagramTransportPlanTemplate::create(
        sessionKey, deployment,
        {{1, family, parsed.value().host, parsed.value().port}});
}

} // namespace media::ffmpeg::graph

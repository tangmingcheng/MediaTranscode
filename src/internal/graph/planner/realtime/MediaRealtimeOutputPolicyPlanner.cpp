#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validRtpPort(std::size_t port) noexcept
{
    return port > 0 && port <= 65534 && (port % 2) == 0;
}

std::string rtpUrl(const std::string& host, std::size_t port)
{
    return "rtp://" + host + ":" + std::to_string(port) + "?localrtpport=0&localrtcpport=0";
}

::media::Result<int> checkedSocketInteger(
    std::uint64_t value, const char* fact)
{
    if (value == 0 || value > static_cast<std::uint64_t>(
            std::numeric_limits<int>::max())) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " exceeds the socket option range"));
    }
    return ::media::Result<int>::success(static_cast<int>(value));
}

void applyPacing(
    MediaRealtimeScheduledRtpOutputPlanningDraft& output,
    const MediaRealtimeDeploymentEnvelopeEncoding& deployment) noexcept
{
    output.writePacingEnabled = true;
    output.writePacingBytesPerSecond = static_cast<std::int64_t>(
        deployment.service.sustainedWireBytesPerSecond);
    output.writePacingBurstBytes = static_cast<std::int64_t>(
        deployment.service.burstWireBytes);
}

std::optional<int> resolvedAudioBitrateKbps(
    const MediaRealtimeRtpTranscodePlanningDraft& plan)
{
    if (!plan.audioPlan || !plan.audioPlan->resolvedOutput ||
        !plan.audioPlan->resolvedOutput->bitrateKbps() ||
        *plan.audioPlan->resolvedOutput->bitrateKbps() <= 0) {
        return std::nullopt;
    }
    return *plan.audioPlan->resolvedOutput->bitrateKbps();
}

::media::Result<MediaRtpRemoteEndpointPair> rtpTransport(
    const std::string& host,
    std::size_t rtpPort)
{
    const bool bracketedIpv6 = host.size() > 2 && host.front() == '[' &&
        host.back() == ']';
    const std::string numericHost = bracketedIpv6
        ? host.substr(1, host.size() - 2)
        : host;
    if (!validRtpPort(rtpPort)) {
        return ::media::Result<MediaRtpRemoteEndpointPair>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP transport facts are incomplete"));
    }
    return MediaRtpRemoteEndpointPair::create(
        bracketedIpv6 ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
        numericHost,
        static_cast<std::uint16_t>(rtpPort),
        static_cast<std::uint16_t>(rtpPort + 1));
}

} // namespace

::media::Result<MediaRealtimeOutputUrls> MediaRealtimeOutputPolicyPlanner::planUrls(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    MediaRealtimeOutputUrls urls;
    if (MediaRealtimeRequestClassifier::udpOutput(request)) {
        if (!MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
            return ::media::Result<MediaRealtimeOutputUrls>::failure(
                ::media::ErrorInfo::unsupported(
                    "UDP output supports only MPEG-TS muxed encapsulation"));
        }
        if (request.output.url.empty()) {
            return ::media::Result<MediaRealtimeOutputUrls>::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS muxed output requires explicit output URL"));
        }
        auto endpoint = parseRtpUdpUrlEndpoint(request.output.url);
        if (!endpoint || endpoint.value().scheme != "udp") {
            return ::media::Result<MediaRealtimeOutputUrls>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS output requires an explicit udp://host:port endpoint"));
        }
        urls.video = request.output.url;
        urls.muxed = request.output.url;
        return ::media::Result<MediaRealtimeOutputUrls>::success(std::move(urls));
    }

    if (!MediaRealtimeRequestClassifier::rtpAvpOutput(request)) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime output transport must be explicit"));
    }
    if (!request.output.url.empty()) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output requires host/basePort; single output URL is unsupported"));
    }
    if (request.output.host.empty() || !request.output.basePort) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output host and base port are required"));
    }
    const std::size_t videoPort = *request.output.basePort;
    if (!validRtpPort(videoPort)) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output ports must be valid even RTP ports"));
    }
    const MediaTranscodeStreamSet streamSet = *request.parameters.execution.streamSet;
    if (streamSet == MediaTranscodeStreamSet::AudioVideo && videoPort > 65532) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime RTP audio output port collides with the video RTP/RTCP pair"));
    }
    const std::size_t audioPort = streamSet == MediaTranscodeStreamSet::AudioVideo
        ? videoPort + 2
        : 0;
    if (streamSet == MediaTranscodeStreamSet::AudioVideo &&
        !validRtpPort(audioPort)) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output ports must be valid even RTP ports"));
    }
    urls.video = rtpUrl(request.output.host, videoPort);
    if (streamSet == MediaTranscodeStreamSet::AudioVideo) {
        urls.audio = rtpUrl(request.output.host, audioPort);
    }
    if (MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        urls.muxed = urls.video;
    }
    return ::media::Result<MediaRealtimeOutputUrls>::success(std::move(urls));
}

::media::Status MediaRealtimeOutputPolicyPlanner::apply(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeOutputUrls& urls,
    MediaRealtimeRtpTranscodePlanningDraft& plan,
    MediaRealtimeOutputPlanningDraft& output)
{
    if (!request.deployment) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Realtime output policy requires the validated deployment envelope"));
    }
    const auto& deployment = request.deployment->encode();
    const std::uint64_t maximumDatagram = (std::min)(
        deployment.mtu.senderMaximumPayloadBytes,
        deployment.mtu.maximumIpPacketBytes - deployment.mtu.ipHeaderBytes -
            deployment.mtu.transportHeaderBytes);
    auto packetSize = checkedSocketInteger(
        maximumDatagram, "planned maximum Datagram payload");
    if (!packetSize ||
        deployment.service.sustainedWireBytesPerSecond >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        deployment.service.burstWireBytes >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return ::media::Status::failure(
            !packetSize ? packetSize.error() :
            ::media::ErrorInfo::invalidArgument(
                "deployment service curve exceeds the runtime numeric range"));
    }
    if (request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        output.muxedOutput.url = urls.muxed;
        output.muxedOutput.mediaId = request.mediaId;
        if (!request.output.transport) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS output requires a resolved transport"));
        }
        output.muxedOutput.transportDecodeLead =
            deployment.latency.maximumResidence;
        if (MediaRealtimeRequestClassifier::rtpAvpOutput(request)) {
            if (!request.output.basePort) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS RTP output requires an explicit endpoint"));
            }
            auto transport = rtpTransport(
                request.output.host, *request.output.basePort);
            if (!transport) {
                return ::media::Status::failure(transport.error());
            }
            output.muxedOutput.rtpTransport =
                std::move(transport).value();
            output.muxedOutput.maximumDatagramBytes =
                static_cast<std::size_t>(packetSize.value());
            output.muxedOutput.sdpPath = request.output.sdpPath;
            output.muxedOutput.scheduledWireBytesPerSecond =
                static_cast<std::int64_t>(
                    deployment.service.sustainedWireBytesPerSecond);
        }
        return ::media::Status::success();
    }

    if (!plan.videoParameters.bitrateKbps) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime output policy requires resolved video bitrate and packet size"));
    }
    const std::string codec = canonicalCodecName(plan.videoPlan.outputCodecName);
    if (codec == "h264" || codec == "avc" || codec == "avc1") plan.videoParameters.globalHeader = true;

    output.videoOutput.url = urls.video;
    output.videoOutput.packetSize = packetSize.value();
    output.videoOutput.mediaId = request.mediaId;
    applyPacing(output.videoOutput, deployment);
    if (output.videoOutput.writePacingBurstBytes >
            std::numeric_limits<int>::max()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP video burst exceeds integer range"));
    }
    auto videoTransport = rtpTransport(
        request.output.host, *request.output.basePort);
    if (!videoTransport) {
        return ::media::Status::failure(videoTransport.error());
    }
    output.videoOutput.scheduledTransport =
        std::move(videoTransport).value();
    if (request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        const auto audioBitrate = resolvedAudioBitrateKbps(plan);
        if (!audioBitrate) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Realtime RTP audio output requires planner-resolved positive audio bitrate"));
        }
        output.audioOutput.url = urls.audio;
        output.audioOutput.packetSize = packetSize.value();
        output.audioOutput.mediaId = request.mediaId;
        applyPacing(output.audioOutput, deployment);
        if (output.audioOutput.writePacingBurstBytes >
                std::numeric_limits<int>::max()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "scheduled RTP audio burst exceeds integer range"));
        }
        auto audioTransport = rtpTransport(
            request.output.host, *request.output.basePort + 2);
        if (!audioTransport) {
            return ::media::Status::failure(audioTransport.error());
        }
        output.audioOutput.scheduledTransport = std::move(audioTransport).value();
    }
    output.sdp.path = request.output.sdpPath;
    output.sdp.mediaId = request.mediaId;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

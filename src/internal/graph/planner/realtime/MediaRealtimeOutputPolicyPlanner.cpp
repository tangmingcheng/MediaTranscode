#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RtpSessionStartupDelayMs = 1000;
constexpr int64_t PacingHeadroomNumerator = 5;
constexpr int64_t PacingHeadroomDenominator = 4;
constexpr int64_t PacingBurstPackets = 2;


bool validRtpPort(std::size_t port) noexcept
{
    return port > 0 && port <= 65534 && (port % 2) == 0;
}

std::string rtpUrl(const std::string& host, std::size_t port)
{
    return "rtp://" + host + ":" + std::to_string(port) + "?localrtpport=0&localrtcpport=0";
}

int64_t pacingBytesPerSecond(int64_t bitsPerSecond) noexcept
{
    const int64_t bytes = (bitsPerSecond + 7) / 8;
    return std::max<int64_t>(1, bytes * PacingHeadroomNumerator / PacingHeadroomDenominator);
}

void applyPacing(MediaRealtimeRtpOutputNodePlan& output, int64_t bitsPerSecond) noexcept
{
    output.writePacingEnabled = true;
    output.writePacingBytesPerSecond = pacingBytesPerSecond(bitsPerSecond);
    output.writePacingBurstBytes = std::max<int64_t>(1, static_cast<int64_t>(output.packetSize) * PacingBurstPackets);
}

MediaLatencyPolicy muxPacing() noexcept
{
    MediaLatencyPolicy policy;
    policy.mode = MediaLatencyMode::Realtime;
    policy.enablePacing = true;
    return policy;
}

::media::Result<MediaRtpUdpSenderConfig> scheduledTransport(
    const std::string& host,
    std::size_t rtpPort,
    const MediaRealtimeRtpOutputNodePlan& output)
{
    const bool bracketedIpv6 = host.size() > 2 && host.front() == '[' &&
        host.back() == ']';
    const std::string numericHost = bracketedIpv6
        ? host.substr(1, host.size() - 2)
        : host;
    if (rtpPort == 0 || rtpPort > 65534 || output.packetSize <= 0 ||
        output.writePacingBurstBytes <= 0 ||
        output.writePacingBurstBytes > std::numeric_limits<int>::max()) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP transport facts are incomplete"));
    }
    return MediaRtpUdpSenderConfig::create(
        bracketedIpv6 ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
        bracketedIpv6 ? "::" : "0.0.0.0",
        numericHost,
        static_cast<std::uint16_t>(rtpPort),
        static_cast<std::uint16_t>(rtpPort + 1),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        static_cast<int>(output.writePacingBurstBytes),
        static_cast<std::size_t>(output.packetSize),
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
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
        urls.muxedFormat = "mpegts";
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
    const bool audioRequested = MediaRealtimeRequestClassifier::audioRequested(request);
    if (audioRequested && videoPort > 65532) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime RTP audio output port collides with the video RTP/RTCP pair"));
    }
    const std::size_t audioPort = audioRequested ? videoPort + 2 : 0;
    if (audioRequested && !validRtpPort(audioPort)) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output ports must be valid even RTP ports"));
    }
    urls.video = rtpUrl(request.output.host, videoPort);
    if (audioRequested) urls.audio = rtpUrl(request.output.host, audioPort);
    if (MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        urls.muxed = urls.video;
        urls.muxedFormat = "mpegts";
    }
    return ::media::Result<MediaRealtimeOutputUrls>::success(std::move(urls));
}

::media::Status MediaRealtimeOutputPolicyPlanner::apply(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeOutputUrls& urls,
    MediaRealtimeRtpTranscodePlan& plan,
    MediaRealtimeOutputPlanningDraft& output)
{
    if (request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        output.muxedOutput.url = urls.muxed;
        output.muxedOutput.format = urls.muxedFormat;
        output.muxedOutput.mediaId = request.mediaId;
        output.muxedOutput.outputResourceKind =
            MediaOutputResourceKind::FFmpegFormatContext;
        output.muxedOutput.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
        output.singleStreamMux.expectVideo = true;
        output.singleStreamMux.expectAudio =
            MediaRealtimeRequestClassifier::audioRequested(request);
        return ::media::Status::success();
    }

    if (!plan.videoParameters.bitrateKbps || !request.output.packetSize) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime output policy requires resolved video bitrate and packet size"));
    }
    const std::string codec = canonicalCodecName(plan.videoPlan.outputCodecName);
    if (codec == "h264" || codec == "avc" || codec == "avc1") plan.videoParameters.globalHeader = true;

    output.videoOutput.url = urls.video;
    output.videoOutput.packetSize = *request.output.packetSize;
    output.videoOutput.mediaId = request.mediaId;
    applyPacing(output.videoOutput, static_cast<int64_t>(*plan.videoParameters.bitrateKbps) * 1000);
    if (MediaRealtimeRequestClassifier::audioRequested(request)) {
        if (!request.parameters.audio.bitrateKbps || *request.parameters.audio.bitrateKbps <= 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP audio output requires explicit positive audio bitrate"));
        }
        output.audioOutput.url = urls.audio;
        output.audioOutput.packetSize = *request.output.packetSize;
        output.audioOutput.mediaId = request.mediaId;
        applyPacing(output.audioOutput, static_cast<int64_t>(*request.parameters.audio.bitrateKbps) * 1000);
        auto videoTransport = scheduledTransport(
            request.output.host, *request.output.basePort, output.videoOutput);
        auto audioTransport = scheduledTransport(
            request.output.host, *request.output.basePort + 2, output.audioOutput);
        if (!videoTransport || !audioTransport) {
            return ::media::Status::failure(
                videoTransport ? audioTransport.error() : videoTransport.error());
        }
        output.videoOutput.scheduledTransport = std::move(videoTransport).value();
        output.audioOutput.scheduledTransport = std::move(audioTransport).value();
    }
    output.sdp.path = request.output.sdpPath;
    output.sdp.mediaId = request.mediaId;
    output.sdp.expectedContexts = MediaRealtimeRequestClassifier::audioRequested(request) ? 2 : 1;
    output.singleStreamMux.expectVideo = true;
    output.singleStreamMux.expectAudio = false;
    output.singleStreamMux.pacingPolicy = muxPacing();
    output.singleStreamMux.monotonicPacketTimestamps = true;
    output.singleStreamMux.startupDelayMs = RtpSessionStartupDelayMs;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

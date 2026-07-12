#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <sstream>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace media::ffmpeg::graph {
namespace {

constexpr int RawVideoStreamIndex = 0;
constexpr int RawAudioStreamIndex = 0;
constexpr int RtpReceiveBufferBytes = 4 * 1024 * 1024;
constexpr int RtpMaximumDatagramBytes = 65'535;
constexpr std::size_t RtpReorderWindowPackets = 64;
constexpr int RtpMaximumReorderDelayMs = 100;
constexpr int RtcpSenderReportTimeoutMs = 3'000;
constexpr int RtcpCnameTimeoutMs = 5'000;

struct NumericUnicastAddress final {
    MediaIpAddressFamily family;
    std::string text;
};

::media::Result<NumericUnicastAddress> numericUnicastAddress(
    const std::string& host, const std::string& owner)
{
    const bool bracketed = host.size() >= 2 && host.front() == '[' && host.back() == ']';
    const std::string text = bracketed ? host.substr(1, host.size() - 2) : host;
    in_addr ipv4{};
    if (!bracketed && inet_pton(AF_INET, text.c_str(), &ipv4) == 1) {
        const uint32_t value = ntohl(ipv4.s_addr);
        const uint8_t first = static_cast<uint8_t>(value >> 24);
        if (first == 0 || first >= 224) {
            return ::media::Result<NumericUnicastAddress>::failure(
                ::media::ErrorInfo::invalidArgument(owner + " requires numeric unicast address"));
        }
        return ::media::Result<NumericUnicastAddress>::success(
            NumericUnicastAddress{MediaIpAddressFamily::Ipv4, text});
    }
    in6_addr ipv6{};
    if (bracketed && inet_pton(AF_INET6, text.c_str(), &ipv6) == 1) {
        if (IN6_IS_ADDR_MULTICAST(&ipv6) || IN6_IS_ADDR_UNSPECIFIED(&ipv6)) {
            return ::media::Result<NumericUnicastAddress>::failure(
                ::media::ErrorInfo::invalidArgument(owner + " requires numeric unicast address"));
        }
        return ::media::Result<NumericUnicastAddress>::success(
            NumericUnicastAddress{MediaIpAddressFamily::Ipv6, text});
    }
    return ::media::Result<NumericUnicastAddress>::failure(
        ::media::ErrorInfo::invalidArgument(owner + " requires numeric IPv4 or bracketed IPv6 unicast address"));
}

::media::Result<MediaRtpUrlEndpoint> endpoint(
    const MediaRealtimeRtpInputMetadata& metadata,
    const std::string& owner)
{
    auto parsed = parseRtpUdpUrlEndpoint(metadata.url);
    if (!parsed) return ::media::Result<MediaRtpUrlEndpoint>::failure(parsed.error());
    const std::size_t port = parsed.value().port;
    if (port == 0 || port > 65534 || (port % 2) != 0) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " port must be an even port in range 1..65534"));
    }
    auto address = numericUnicastAddress(parsed.value().host, owner);
    if (!address) return ::media::Result<MediaRtpUrlEndpoint>::failure(address.error());
    parsed.value().host = std::move(address.value().text);
    return parsed;
}

MediaRealtimeRtpTransportPlan transportPlan(
    const MediaRtpUrlEndpoint& endpoint,
    const MediaRealtimeRtpInputMetadata& metadata,
    const MediaRealtimeRtpCodecDescriptor& descriptor,
    int cancellableReadTimeoutMs)
{
    const bool ipv6 = endpoint.host.find(':') != std::string::npos;
    return MediaRealtimeRtpTransportPlan{
        ipv6 ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
        endpoint.host,
        endpoint.port,
        static_cast<uint16_t>(endpoint.port + 1),
        static_cast<uint8_t>(*metadata.payloadType),
        descriptor.clockRate,
        RtpReceiveBufferBytes,
        RtpMaximumDatagramBytes,
        RtpReorderWindowPackets,
        RtpMaximumReorderDelayMs,
        cancellableReadTimeoutMs,
        true,
        true,
        RtcpSenderReportTimeoutMs,
        RtcpCnameTimeoutMs,
        MediaRtcpCompositionMode::StrictCompoundRfc3550
    };
}

MediaRtpDepacketizerConfig depacketizerPlan(
    MediaStreamKind streamKind,
    const MediaRealtimeRtpInputMetadata& metadata,
    const MediaRealtimeRtpCodecDescriptor& descriptor)
{
    return MediaRtpDepacketizerConfig{
        streamKind,
        descriptor.codecName,
        metadata.fmtp,
        static_cast<uint8_t>(*metadata.payloadType),
        descriptor.clockRate,
        descriptor.channels,
        descriptor.accessUnitDurationRtpTicks
    };
}

void fillNodePlan(
    const MediaRealtimeRtpTranscodeRequest& request,
    std::string url,
    std::string sdpText,
    std::optional<MediaRealtimeRtpTransportPlan> transport,
    std::optional<MediaRtpDepacketizerConfig> depacketizer,
    MediaRealtimeRtpInputNodePlan& node)
{
    node.url = std::move(url);
    node.sdpText = std::move(sdpText);
    node.rtspTransport = request.input.rtspTransport;
    node.openTimeoutMs = *request.input.openTimeoutMs;
    node.readTimeoutMs = *request.input.readTimeoutMs;
    node.analyzeDurationUs = *request.input.analyzeDurationUs;
    node.probeSizeBytes = *request.input.probeSizeBytes;
    node.lowLatency = *request.input.lowLatency;
    node.mediaId = request.mediaId;
    node.rtpTransport = std::move(transport);
    node.rtpDepacketizer = std::move(depacketizer);
}

} // namespace

::media::Result<MediaRealtimeRawInputPlan> MediaRealtimeInputPlanner::planRawRtp(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.readTimeoutMs || *request.input.readTimeoutMs <= 0) {
        return ::media::Result<MediaRealtimeRawInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Raw RTP input requires an explicit positive read timeout"));
    }
    auto videoDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Video, request.input.videoRtp);
    if (!videoDescriptor) return ::media::Result<MediaRealtimeRawInputPlan>::failure(videoDescriptor.error());
    auto videoEndpoint = endpoint(request.input.videoRtp, "Raw RTP video");
    if (!videoEndpoint) return ::media::Result<MediaRealtimeRawInputPlan>::failure(videoEndpoint.error());

    MediaRealtimeRawInputPlan result;
    result.videoUrl = request.input.videoRtp.url;
    result.videoSdp.clear();
    result.video.streamIndex = RawVideoStreamIndex;
    result.video.codecName = videoDescriptor.value().codecName;
    result.videoTransport = transportPlan(
        videoEndpoint.value(), request.input.videoRtp, videoDescriptor.value(),
        *request.input.readTimeoutMs);
    result.videoDepacketizer = depacketizerPlan(MediaStreamKind::Video, request.input.videoRtp, videoDescriptor.value());
    if (auto status = MediaRtpDepacketizerFactory::validate(result.videoDepacketizer); !status) {
        return ::media::Result<MediaRealtimeRawInputPlan>::failure(status.error());
    }

    if (MediaRealtimeRequestClassifier::audioRequested(request)) {
        auto audioDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Audio, request.input.audioRtp);
        if (!audioDescriptor) return ::media::Result<MediaRealtimeRawInputPlan>::failure(audioDescriptor.error());
        auto audioEndpoint = endpoint(request.input.audioRtp, "Raw RTP audio");
        if (!audioEndpoint) return ::media::Result<MediaRealtimeRawInputPlan>::failure(audioEndpoint.error());
        result.audioUrl = request.input.audioRtp.url;
        result.audioSdp.clear();
        MediaInputAudioStreamInfo audio;
        audio.streamIndex = RawAudioStreamIndex;
        audio.codecName = audioDescriptor.value().codecName;
        audio.sampleRate = audioDescriptor.value().clockRate;
        audio.channels = audioDescriptor.value().channels;
        audio.bitrateBitsPerSecond = request.input.audioRtp.bitrateKbps
            ? static_cast<int64_t>(*request.input.audioRtp.bitrateKbps) * 1000
            : 0;
        result.audio = std::move(audio);
        result.audioTransport = transportPlan(
            audioEndpoint.value(), request.input.audioRtp, audioDescriptor.value(),
            *request.input.readTimeoutMs);
        result.audioDepacketizer = depacketizerPlan(MediaStreamKind::Audio, request.input.audioRtp, audioDescriptor.value());
        if (auto status = MediaRtpDepacketizerFactory::validate(*result.audioDepacketizer); !status) {
            return ::media::Result<MediaRealtimeRawInputPlan>::failure(status.error());
        }
    }
    return ::media::Result<MediaRealtimeRawInputPlan>::success(std::move(result));
}

void MediaRealtimeInputPlanner::applyNodePlans(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeRawInputPlan* raw,
    MediaRealtimeRtpTranscodePlan& plan)
{
    fillNodePlan(request,
                 raw ? raw->videoUrl : request.input.url,
                 raw ? raw->videoSdp : std::string{},
                 raw ? std::optional<MediaRealtimeRtpTransportPlan>(raw->videoTransport) : std::nullopt,
                 raw ? std::optional<MediaRtpDepacketizerConfig>(raw->videoDepacketizer) : std::nullopt,
                 plan.input);
    if (raw && raw->audio) {
        plan.useIsolatedAudioInput = true;
        fillNodePlan(request, raw->audioUrl, raw->audioSdp, raw->audioTransport, raw->audioDepacketizer, plan.audioInput);
    }
}

::media::Result<MediaPreparedRealtimeInputScan> MediaRealtimeInputPlanner::prepare(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaPipelinePlannerOptions& options,
    const MediaRealtimeInputOpener* opener)
{
    return opener
        ? MediaPipelineCapabilityScanner::prepareRealtimeInput(
              request.input.url, options, MediaRealtimeRequestClassifier::audioRequested(request), *opener)
        : MediaPipelineCapabilityScanner::prepareRealtimeInput(
              request.input.url, options, MediaRealtimeRequestClassifier::audioRequested(request));
}

} // namespace media::ffmpeg::graph

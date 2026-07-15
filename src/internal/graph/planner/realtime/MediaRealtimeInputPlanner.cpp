#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <sstream>
#include <limits>
#include <utility>

extern "C" {
#include <libavutil/dict.h>
}

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
constexpr std::size_t TsPacketSize = 188;
constexpr std::uint64_t TsMaximumPacketPositionRegressionBytes = 1024 * 1024;

::media::Result<MediaPreparedRealtimeInputScan> prepareMpegTs(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsInputSessionOpener* opener)
{
    const auto probeBytes = static_cast<std::uint64_t>(*request.input.probeSizeBytes);
    auto capacity = MediaRealtimeTsInputPlan::minimumEvidenceCapacity(
        TsPacketSize, probeBytes, TsMaximumPacketPositionRegressionBytes);
    if (!capacity) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(capacity.error());
    const auto policy = MediaRealtimeTsInputPlan::create(
        TsPacketSize, probeBytes, TsMaximumPacketPositionRegressionBytes,
        capacity.value(), MediaRealtimeRequestClassifier::audioRequested(request) ? 2 : 1);
    if (!policy) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(policy.error());

    MediaTsInputSessionOptions options;
    options.protocolUrl = request.input.url;
    AVDictionary* protocolOptions = nullptr;
    AVDictionary* demuxOptions = nullptr;
    const auto microseconds = [](int milliseconds) {
        return std::to_string(static_cast<std::int64_t>(milliseconds) * 1000);
    };
    av_dict_set(&protocolOptions, "stimeout",
                microseconds(*request.input.openTimeoutMs).c_str(), 0);
    av_dict_set(&protocolOptions, "rw_timeout",
                microseconds(*request.input.readTimeoutMs).c_str(), 0);
    av_dict_set(&protocolOptions, "timeout",
                microseconds(*request.input.readTimeoutMs).c_str(), 0);
    av_dict_set(&demuxOptions, "analyzeduration",
                std::to_string(*request.input.analyzeDurationUs).c_str(), 0);
    av_dict_set(&demuxOptions, "probesize",
                std::to_string(*request.input.probeSizeBytes).c_str(), 0);
    if (*request.input.lowLatency) {
        av_dict_set(&demuxOptions, "fflags", "nobuffer", 0);
        av_dict_set(&demuxOptions, "flags", "low_delay", 0);
    }
    options.protocolOptions = protocolOptions;
    options.demuxOptions = demuxOptions;
    options.avioBufferBytes = policy.value().avioBufferBytes;
    options.packetStride = policy.value().packetSize;
    options.evidenceCapacity = policy.value().evidenceTimelineCapacity;
    options.pesProvenanceCapacity = policy.value().pesProvenanceCapacity;
    options.maximumPositionRegressionBytes =
        policy.value().maximumPacketPositionRegressionBytes;
    auto opened = opener ? (*opener)(options) : MediaTsInputSession::open(options);
    av_dict_free(&protocolOptions);
    av_dict_free(&demuxOptions);
    if (!opened) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(opened.error());
    auto session = std::move(opened).value();

    const FFmpegInputStreamSnapshot* video = nullptr;
    const FFmpegInputStreamSnapshot* audio = nullptr;
    for (const auto& stream : session->streamSnapshots()) {
        if (!video && stream.streamKind == MediaStreamKind::Video) video = &stream;
        if (!audio && stream.streamKind == MediaStreamKind::Audio) audio = &stream;
    }
    if (!video || (MediaRealtimeRequestClassifier::audioRequested(request) && !audio)) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS selected A/V streams are incomplete"));
    }
    auto selected = MediaTsProgramSelector::select(
        session->programSnapshots(), session->programInventory(),
        video->index, audio ? audio->index : -1);
    if (!selected) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(selected.error());
    if (!audio) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::unsupported(
                "MPEG-TS PES provenance currently requires planned audio and video"));
    }
    MediaTsRuntimeBinding runtimeBinding;
    runtimeBinding.originPolicy = policy.value().packetOriginPolicy;
    runtimeBinding.video = MediaTsRuntimeStreamBinding{
        video->index, static_cast<std::uint16_t>(selected.value().videoPid)};
    runtimeBinding.audio = MediaTsRuntimeStreamBinding{
        audio->index, static_cast<std::uint16_t>(selected.value().audioPid)};
    runtimeBinding.pcrPid = static_cast<std::uint16_t>(selected.value().pcrPid);
    runtimeBinding.pesProvenanceCapacity = policy.value().pesProvenanceCapacity;
    if (auto configured = session->configureRuntimeBinding(runtimeBinding); !configured) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(configured.error());
    }
    MediaPreparedRealtimeInputScan result;
    result.streams.video.streamIndex = video->index;
    result.streams.video.codecName = video->format.codec.codecName;
    result.streams.video.width = video->format.video.size.width;
    result.streams.video.height = video->format.video.size.height;
    result.streams.video.bitrateBitsPerSecond = video->format.codec.bitrate;
    result.streams.video.frameRate = video->format.video.frameRate;
    if (audio) {
        result.streams.hasAudio = true;
        result.streams.audio.streamIndex = audio->index;
        result.streams.audio.codecName = audio->format.codec.codecName;
        result.streams.audio.sampleRate = audio->format.audio.sampleRate;
        result.streams.audio.channels = audio->format.audio.channels;
        result.streams.audio.channelLayout = audio->format.audio.channelLayout;
        result.streams.audio.sampleFormat = audio->format.audio.sampleFormat;
        auto profile = MediaAudioProfile::fromCodecProfile(
            audio->format.codec.codecName, audio->format.codec.profile);
        if (!profile) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(profile.error());
        result.streams.audio.profile = profile.value();
        result.streams.audio.bitrateBitsPerSecond = audio->format.codec.bitrate;
        auto codecParameters = audio->cloneCodecParameters();
        if (!codecParameters) {
            return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
                codecParameters.error());
        }
        auto decoder = MediaAudioDecoderCapabilityProvider::verifyAacAdts(
            *codecParameters.value());
        if (!decoder) {
            return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
                decoder.error());
        }
        result.streams.audio.maximumAccessUnitSamples = static_cast<int>(
            decoder.value().maximumOutputBlockInputSamples);
        result.streams.audio.selectedDecoder = std::move(decoder).value();
    }
    auto prepared = MediaPreparedRealtimeInput::createMpegTs(std::move(session));
    if (!prepared) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(prepared.error());
    result.prepared = std::move(prepared).value();
    result.selectedTsProgram = selected.value();
    return ::media::Result<MediaPreparedRealtimeInputScan>::success(std::move(result));
}

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
        audio.channelLayout = audio.channels == 1 ? "mono" : "stereo";
        audio.sampleFormat = "unknown";
        audio.profile = audioDescriptor.value().audioProfile;
        audio.bitrateBitsPerSecond = request.input.audioRtp.bitrateKbps
            ? static_cast<int64_t>(*request.input.audioRtp.bitrateKbps) * 1000
            : 0;
        ::media::Result<MediaSelectedAudioDecoder> decoder =
            ::media::Result<MediaSelectedAudioDecoder>::failure(
                ::media::ErrorInfo::unsupported(
                    "Raw RTP audio decoder capability is not supported"));
        if (audioDescriptor.value().codecName == "aac") {
            auto fmtp = parseRtpFmtp(request.input.audioRtp.fmtp);
            if (!fmtp) {
                return ::media::Result<MediaRealtimeRawInputPlan>::failure(
                    fmtp.error());
            }
            const auto config = fmtp.value().find("config");
            if (config == fmtp.value().end()) {
                return ::media::Result<MediaRealtimeRawInputPlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Raw RTP AAC decoder configuration is missing"));
            }
            auto configBytes = decodeRtpFmtpHex(config->second);
            if (!configBytes) {
                return ::media::Result<MediaRealtimeRawInputPlan>::failure(
                    configBytes.error());
            }
            decoder = MediaAudioDecoderCapabilityProvider::
                verifyAacAudioSpecificConfig(
                    audio.sampleRate, audio.channels, configBytes.value());
        } else if (audioDescriptor.value().codecName == "opus") {
            decoder = MediaAudioDecoderCapabilityProvider::verifyOpusRtp(
                audio.sampleRate, audio.channels,
                audioDescriptor.value().maximumAccessUnitDurationRtpTicks);
        }
        if (!decoder) {
            return ::media::Result<MediaRealtimeRawInputPlan>::failure(
                decoder.error());
        }
        if (decoder.value().maximumOutputBlockInputSamples !=
            audioDescriptor.value().maximumAccessUnitDurationRtpTicks) {
            return ::media::Result<MediaRealtimeRawInputPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "selected audio decoder block conflicts with RTP packetization"));
        }
        audio.maximumAccessUnitSamples = static_cast<int>(
            decoder.value().maximumOutputBlockInputSamples);
        audio.selectedDecoder = std::move(decoder).value();
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
    const MediaRealtimePreflightIo* io)
{
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) {
        if (io && !io->openMpegTs) {
            return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS preflight opener is required"));
        }
        return prepareMpegTs(request, io ? &io->openMpegTs : nullptr);
    }
    return io
        ? MediaPipelineCapabilityScanner::prepareRealtimeInput(
              request.input.url, options, MediaRealtimeRequestClassifier::audioRequested(request), io->openGeneric)
        : MediaPipelineCapabilityScanner::prepareRealtimeInput(
              request.input.url, options, MediaRealtimeRequestClassifier::audioRequested(request));
}

} // namespace media::ffmpeg::graph

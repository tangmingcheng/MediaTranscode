#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"

#include "internal/graph/planner/MediaRtpClockLivenessPolicy.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
#include <sstream>
#include <limits>
#include <type_traits>
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

constexpr int RtpReceiveBufferBytes = 4 * 1024 * 1024;
constexpr int RtpMaximumDatagramBytes = 65'535;
constexpr std::size_t RtpReorderWindowPackets = 64;
constexpr int RtpMaximumReorderDelayMs = 100;
constexpr std::size_t TsPacketSize = 188;
constexpr std::uint64_t TsMaximumPacketPositionRegressionBytes = 1024 * 1024;
constexpr std::int64_t Millisecond = 1'000'000;

struct MediaRtpInputClockTransportPolicy final {
    bool requireSenderReports;
    bool requireCname;
    int senderReportTimeoutMs;
    int identityEvidenceTimeoutMs;
    MediaRtpClockLossPolicy lossPolicy;
    MediaRtpClockLossPolicy secondaryLossPolicy;
    MediaRtcpCompositionMode rtcpCompositionMode;
};

struct MediaTsSessionOpenSettings final {
    std::string protocolUrl;
    int openTimeoutMs = 0;
    int readTimeoutMs = 0;
    int analyzeDurationUs = 0;
    int probeSizeBytes = 0;
    bool lowLatency = false;
    std::size_t avioBufferBytes = 0;
    std::size_t packetStride = 0;
    std::size_t evidenceCapacity = 0;
    std::size_t pesProvenanceCapacity = 0;
    std::uint64_t maximumPositionRegressionBytes = 0;
};

::media::Result<std::unique_ptr<MediaTsInputSession>> openMpegTsPreflightSession(
    const MediaTsSessionOpenSettings& settings,
    const MediaTsInputSessionOpener& opener)
{
    MediaTsInputSessionOptions options;
    options.protocolUrl = settings.protocolUrl;
    AVDictionary* protocolOptions = nullptr;
    AVDictionary* demuxOptions = nullptr;
    const auto microseconds = [](int milliseconds) {
        return std::to_string(static_cast<std::int64_t>(milliseconds) * 1000);
    };
    av_dict_set(&protocolOptions, "stimeout",
                microseconds(settings.openTimeoutMs).c_str(), 0);
    av_dict_set(&protocolOptions, "rw_timeout",
                microseconds(settings.readTimeoutMs).c_str(), 0);
    av_dict_set(&protocolOptions, "timeout",
                microseconds(settings.readTimeoutMs).c_str(), 0);
    av_dict_set(&demuxOptions, "analyzeduration",
                std::to_string(settings.analyzeDurationUs).c_str(), 0);
    av_dict_set(&demuxOptions, "probesize",
                std::to_string(settings.probeSizeBytes).c_str(), 0);
    if (settings.lowLatency) {
        av_dict_set(&demuxOptions, "fflags", "nobuffer", 0);
        av_dict_set(&demuxOptions, "flags", "low_delay", 0);
    }
    options.protocolOptions = protocolOptions;
    options.demuxOptions = demuxOptions;
    options.avioBufferBytes = settings.avioBufferBytes;
    options.packetStride = settings.packetStride;
    options.evidenceCapacity = settings.evidenceCapacity;
    options.pesProvenanceCapacity = settings.pesProvenanceCapacity;
    options.maximumPositionRegressionBytes =
        settings.maximumPositionRegressionBytes;
    auto opened = opener
        ? opener(options)
        : MediaTsInputSession::open(options);
    av_dict_free(&protocolOptions);
    av_dict_free(&demuxOptions);
    return opened;
}

::media::Result<std::unique_ptr<MediaTsDemuxSession>>
openMpegTsRuntimeSession(
    const MediaTsSessionOpenSettings& settings,
    const MediaTsRuntimeBinding& plannedBinding,
    const MediaTsInputSessionOpener& opener)
{
    auto opened = openMpegTsPreflightSession(settings, opener);
    if (!opened) {
        return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::failure(
            opened.error());
    }
    auto session = std::move(opened).value();
    std::vector<MediaTsRuntimeStreamFacts> runtimeStreamFacts;
    runtimeStreamFacts.reserve(session->streamSnapshots().size());
    for (const auto& stream : session->streamSnapshots()) {
        runtimeStreamFacts.push_back(MediaTsRuntimeStreamFacts{
            stream.index, stream.streamKind, stream.time.timeBase});
    }
    auto runtimeBinding = MediaTsRuntimeBindingCodec::rebindStreamIndexes(
        plannedBinding, session->programSnapshots(),
        session->programInventory(), runtimeStreamFacts);
    if (!runtimeBinding) {
        return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::failure(
            runtimeBinding.error());
    }
    if (auto configured = session->configureRuntimeBinding(
            runtimeBinding.value());
        !configured) {
        return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::failure(
            configured.error());
    }
    return ::media::Result<std::unique_ptr<MediaTsDemuxSession>>::success(
        std::move(session));
}

::media::Result<int> wholeMilliseconds(
    const std::optional<MediaRunningTime>& duration,
    const char* field)
{
    if (!duration || duration->nanoseconds() <= 0 ||
        (duration->nanoseconds() % Millisecond) != 0 ||
        duration->nanoseconds() / Millisecond >
            (std::numeric_limits<int>::max)()) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Raw RTP A/V sync requires millisecond-aligned ") +
                field));
    }
    return ::media::Result<int>::success(
        static_cast<int>(duration->nanoseconds() / Millisecond));
}

::media::Result<MediaRtpInputClockTransportPolicy> clockTransportPolicy(
    const MediaAvSyncPlan* avSync)
{
    if (!avSync) {
        return ::media::Result<MediaRtpInputClockTransportPolicy>::success({
            true,
            false,
            MediaRtpClockLivenessPolicy::SenderReportTimeoutMs,
            MediaRtpClockLivenessPolicy::CnameTimeoutMs,
            MediaRtpClockLossPolicy::FailOnExpired,
            MediaRtpClockLossPolicy::FailOnExpired,
            MediaRtcpCompositionMode::ReducedSizeRfc5506});
    }
    if (!avSync->rtpInput || !avSync->rtpInput->input.requireSenderReports ||
        !avSync->rtpInput->input.rtcpCompositionMode ||
        !avSync->rtpInput->input.clockLossPolicy ||
        !avSync->rtpInput->input.secondaryClockLossPolicy) {
        return ::media::Result<MediaRtpInputClockTransportPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "Raw RTP A/V sync requires a complete planner-owned RTCP policy"));
    }
    const auto senderReportTimeout = wholeMilliseconds(
        avSync->rtpInput->input.senderReportTimeoutNs,
        "sender report timeout");
    if (!senderReportTimeout) {
        return ::media::Result<MediaRtpInputClockTransportPolicy>::failure(
            senderReportTimeout.error());
    }
    const auto identityEvidenceTimeout = wholeMilliseconds(
        avSync->rtpInput->input.identityEvidenceTimeoutNs,
        "identity evidence timeout");
    if (!identityEvidenceTimeout) {
        return ::media::Result<MediaRtpInputClockTransportPolicy>::failure(
            identityEvidenceTimeout.error());
    }
    if (avSync->rtpInput->input.streamAssociationMode !=
        MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair) {
        return ::media::Result<MediaRtpInputClockTransportPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "Raw RTP A/V sync stream association mode is missing"));
    }
    return ::media::Result<MediaRtpInputClockTransportPolicy>::success({
        *avSync->rtpInput->input.requireSenderReports,
        false,
        senderReportTimeout.value(),
        identityEvidenceTimeout.value(),
        *avSync->rtpInput->input.clockLossPolicy,
        *avSync->rtpInput->input.secondaryClockLossPolicy,
        *avSync->rtpInput->input.rtcpCompositionMode});
}

::media::Result<MediaPreparedRealtimeInputScan> prepareMpegTs(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsInputSessionOpener* opener)
{
    const auto probeBytes = static_cast<std::uint64_t>(*request.input.probeSizeBytes);
    auto capacity = MediaRealtimeTsInputPolicy::minimumEvidenceCapacity(
        TsPacketSize, probeBytes, TsMaximumPacketPositionRegressionBytes);
    if (!capacity) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(capacity.error());
    const auto policy = MediaRealtimeTsInputPolicy::create(
        TsPacketSize, probeBytes, TsMaximumPacketPositionRegressionBytes,
        capacity.value(),
        request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo ? 2 : 1);
    if (!policy) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(policy.error());

    const MediaTsSessionOpenSettings openSettings{
        request.input.url,
        *request.input.openTimeoutMs,
        *request.input.readTimeoutMs,
        *request.input.analyzeDurationUs,
        *request.input.probeSizeBytes,
        *request.input.lowLatency,
        policy.value().avioBufferBytes,
        policy.value().packetSize,
        policy.value().evidenceTimelineCapacity,
        policy.value().pesProvenanceCapacity,
        policy.value().maximumPacketPositionRegressionBytes};
    const MediaTsInputSessionOpener sessionOpener =
        opener ? *opener : MediaTsInputSessionOpener{};
    auto opened = openMpegTsPreflightSession(openSettings, sessionOpener);
    if (!opened) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(opened.error());
    auto session = std::move(opened).value();

    const FFmpegInputStreamSnapshot* video = nullptr;
    const FFmpegInputStreamSnapshot* audio = nullptr;
    for (const auto& stream : session->streamSnapshots()) {
        if (!video && stream.streamKind == MediaStreamKind::Video) video = &stream;
        if (request.parameters.execution.streamSet ==
                MediaTranscodeStreamSet::AudioVideo &&
            !audio && stream.streamKind == MediaStreamKind::Audio) {
            audio = &stream;
        }
    }
    if (!video ||
        (request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo &&
         !audio)) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS selected A/V streams are incomplete"));
    }
    ::media::Result<MediaTsProgramSelection> selected =
        request.parameters.execution.streamSet ==
            MediaTranscodeStreamSet::AudioVideo
        ? [&]() -> ::media::Result<MediaTsProgramSelection> {
              auto av = MediaTsProgramSelector::selectAudioVideo(
                  session->programSnapshots(), session->programInventory(),
                  video->index, video->time.timeBase,
                  audio->index, audio->time.timeBase);
              if (!av) {
                  return ::media::Result<MediaTsProgramSelection>::failure(
                      av.error());
              }
              return ::media::Result<MediaTsProgramSelection>::success(
                  MediaTsProgramSelection{std::move(av).value()});
          }()
        : [&]() -> ::media::Result<MediaTsProgramSelection> {
              auto videoOnly = MediaTsProgramSelector::selectVideoOnly(
                  session->programSnapshots(), session->programInventory(),
                  video->index, video->time.timeBase);
              if (!videoOnly) {
                  return ::media::Result<MediaTsProgramSelection>::failure(
                      videoOnly.error());
              }
              return ::media::Result<MediaTsProgramSelection>::success(
                  MediaTsProgramSelection{std::move(videoOnly).value()});
          }();
    if (!selected) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            selected.error());
    }
    auto runtimeBinding = MediaTsRuntimeBindingCodec::create(
        selected.value(), policy.value().packetOriginPolicy,
        request.parameters.execution.streamSet ==
                MediaTranscodeStreamSet::VideoOnly
            ? MediaTsUnexpectedElementaryPidPolicy::Ignore
            : MediaTsUnexpectedElementaryPidPolicy::Reject,
        policy.value().pesProvenanceCapacity);
    if (!runtimeBinding) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            runtimeBinding.error());
    }
    if (auto configured = session->configureRuntimeBinding(
            runtimeBinding.value());
        !configured) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(configured.error());
    }
    const std::size_t durationProbeSelectedPacketLimit =
        static_cast<std::size_t>(
        probeBytes / TsPacketSize);
    const std::size_t durationProbeSourcePacketLimit =
        policy.value().pesProvenanceCapacity;
    auto durationProbeBudget = MediaTsDurationProbeBudget::create(
        durationProbeSelectedPacketLimit,
        durationProbeSourcePacketLimit);
    if (!durationProbeBudget) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            durationProbeBudget.error());
    }
    auto durationEvidence = session->probeSelectedPacketDurations(
        durationProbeBudget.value());
    if (!durationEvidence) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            durationEvidence.error());
    }
    auto selectedProgram = std::visit(
        [](const auto& selection, const auto& evidence)
            -> ::media::Result<MediaTsSelectedProgramPlan> {
            using Selection = std::decay_t<decltype(selection)>;
            using Evidence = std::decay_t<decltype(evidence)>;
            if constexpr (
                std::is_same_v<Selection, MediaTsVideoOnlyProgramSelection> &&
                std::is_same_v<Evidence, MediaTsVideoOnlyPacketDurationEvidence>) {
                return ::media::Result<MediaTsSelectedProgramPlan>::success(
                    MediaTsVideoOnlySelectedProgramPlan{
                        selection, evidence.video});
            } else if constexpr (
                std::is_same_v<Selection, MediaTsAudioVideoProgramSelection> &&
                std::is_same_v<Evidence, MediaTsAudioVideoPacketDurationEvidence>) {
                return ::media::Result<MediaTsSelectedProgramPlan>::success(
                    MediaTsAudioVideoSelectedProgramPlan{
                        selection, evidence.video, evidence.audio});
            } else {
                return ::media::Result<MediaTsSelectedProgramPlan>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS duration evidence stream set conflicts with selection"));
            }
        },
        selected.value(), durationEvidence.value());
    if (!selectedProgram) {
        return ::media::Result<MediaPreparedRealtimeInputScan>::failure(
            selectedProgram.error());
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
    const auto plannedBinding = runtimeBinding.value();
    auto prepared = MediaPreparedRealtimeInput::createMpegTs(
        std::move(session),
        [openSettings, plannedBinding, sessionOpener]() mutable {
            return openMpegTsRuntimeSession(
                openSettings, plannedBinding, sessionOpener);
        });
    if (!prepared) return ::media::Result<MediaPreparedRealtimeInputScan>::failure(prepared.error());
    result.prepared = std::move(prepared).value();
    result.selectedTsProgram = std::move(selectedProgram).value();
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
    int cancellableReadTimeoutMs,
    const MediaRtpInputClockTransportPolicy& clockPolicy,
    MediaRtpClockLossPolicy lossPolicy)
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
        clockPolicy.requireSenderReports,
        clockPolicy.requireCname,
        clockPolicy.senderReportTimeoutMs,
        clockPolicy.identityEvidenceTimeoutMs,
        lossPolicy,
        clockPolicy.rtcpCompositionMode
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
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaAvSyncPlan* avSync)
{
    if (!request.input.readTimeoutMs || *request.input.readTimeoutMs <= 0) {
        return ::media::Result<MediaRealtimeRawInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Raw RTP input requires an explicit positive read timeout"));
    }
    if (request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo &&
        !avSync) {
        return ::media::Result<MediaRealtimeRawInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Raw RTP A/V input requires an explicit synchronization plan"));
    }
    auto selectedClockPolicy = clockTransportPolicy(avSync);
    if (!selectedClockPolicy) {
        return ::media::Result<MediaRealtimeRawInputPlan>::failure(
            selectedClockPolicy.error());
    }
    auto videoDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Video, request.input.videoRtp);
    if (!videoDescriptor) return ::media::Result<MediaRealtimeRawInputPlan>::failure(videoDescriptor.error());
    auto videoEndpoint = endpoint(request.input.videoRtp, "Raw RTP video");
    if (!videoEndpoint) return ::media::Result<MediaRealtimeRawInputPlan>::failure(videoEndpoint.error());

    MediaRealtimeRawInputPlan result;
    result.videoUrl = request.input.videoRtp.url;
    result.videoSdp.clear();
    result.video.streamIndex = MediaRealtimeRawInputPlan::VideoStreamIndex;
    result.video.codecName = videoDescriptor.value().codecName;
    result.videoTransport = transportPlan(
        videoEndpoint.value(), request.input.videoRtp, videoDescriptor.value(),
        *request.input.readTimeoutMs, selectedClockPolicy.value(),
        selectedClockPolicy.value().lossPolicy);
    auto videoDepacketizer = MediaRealtimeRtpCodecRegistry::planDepacketizerConfig(
        MediaStreamKind::Video, request.input.videoRtp, videoDescriptor.value());
    if (!videoDepacketizer) {
        return ::media::Result<MediaRealtimeRawInputPlan>::failure(
            videoDepacketizer.error());
    }
    result.videoDepacketizer = std::move(videoDepacketizer).value();

    if (request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        auto audioDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Audio, request.input.audioRtp);
        if (!audioDescriptor) return ::media::Result<MediaRealtimeRawInputPlan>::failure(audioDescriptor.error());
        auto audioEndpoint = endpoint(request.input.audioRtp, "Raw RTP audio");
        if (!audioEndpoint) return ::media::Result<MediaRealtimeRawInputPlan>::failure(audioEndpoint.error());
        result.audioUrl = request.input.audioRtp.url;
        result.audioSdp.clear();
        MediaInputAudioStreamInfo audio;
        audio.streamIndex = MediaRealtimeRawInputPlan::AudioStreamIndex;
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
            auto fmtp = parseRtpFmtp(*request.input.audioRtp.fmtp);
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
            *request.input.readTimeoutMs, selectedClockPolicy.value(),
            selectedClockPolicy.value().secondaryLossPolicy);
        auto audioDepacketizer = MediaRealtimeRtpCodecRegistry::planDepacketizerConfig(
            MediaStreamKind::Audio, request.input.audioRtp, audioDescriptor.value());
        if (!audioDepacketizer) {
            return ::media::Result<MediaRealtimeRawInputPlan>::failure(
                audioDepacketizer.error());
        }
        result.audioDepacketizer = std::move(audioDepacketizer).value();
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
        return prepareMpegTs(
            request, io ? &io->openMpegTs : nullptr);
    }
    return io
        ? MediaPipelineCapabilityScanner::prepareRealtimeInput(
              request.input.url, options, *request.parameters.execution.streamSet,
              io->openGeneric)
        : MediaPipelineCapabilityScanner::prepareRealtimeInput(
              request.input.url, options, *request.parameters.execution.streamSet);
}

::media::Result<MediaPreparedRawRtpProbe>
MediaRealtimeInputPlanner::prepareRawRtpVideo(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    const auto& metadata = request.input.videoRtp;
    if (metadata.fmtp) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP video auto-detection requires omitted manual fmtp"));
    }
    if (!metadata.payloadType || !metadata.clockRate ||
        !request.input.openTimeoutMs || !request.input.readTimeoutMs ||
        !request.input.analyzeDurationUs || !request.input.probeSizeBytes) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP video auto-detection requires explicit RTP identity and probe limits"));
    }
    auto parsedEndpoint = endpoint(metadata, "Raw RTP video probe");
    if (!parsedEndpoint) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            parsedEndpoint.error());
    }
    const bool ipv6 = parsedEndpoint.value().host.find(':') !=
        std::string::npos;
    MediaRawRtpProbePlan plan;
    MediaRawRtpPreparedStreamPlan videoProbeStream{
        MediaRtpUdpTransportConfig{
            ipv6 ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
            parsedEndpoint.value().host,
            parsedEndpoint.value().port,
            static_cast<std::uint16_t>(parsedEndpoint.value().port + 1),
            RtpReceiveBufferBytes,
            RtpMaximumDatagramBytes,
            *request.input.readTimeoutMs,
            nullptr},
        MediaPreparedRawRtpIdentity{
            MediaStreamKind::Video,
            canonicalCodecName(metadata.codecName),
            static_cast<std::uint8_t>(*metadata.payloadType),
            *metadata.clockRate}};
    if (request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        const auto& audio = request.input.audioRtp;
        if (!audio.payloadType || !audio.clockRate) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP automatic signaling requires explicit companion audio identity"));
        }
        auto audioEndpoint = endpoint(audio, "Raw RTP companion audio");
        if (!audioEndpoint) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                audioEndpoint.error());
        }
        const bool audioIpv6 = audioEndpoint.value().host.find(':') !=
            std::string::npos;
        MediaRawRtpPreparedStreamPlan audioProbeStream{
            MediaRtpUdpTransportConfig{
                audioIpv6 ? MediaIpAddressFamily::Ipv6
                          : MediaIpAddressFamily::Ipv4,
                audioEndpoint.value().host,
                audioEndpoint.value().port,
                static_cast<std::uint16_t>(audioEndpoint.value().port + 1),
                RtpReceiveBufferBytes,
                RtpMaximumDatagramBytes,
                *request.input.readTimeoutMs,
                nullptr},
            MediaPreparedRawRtpIdentity{
                MediaStreamKind::Audio,
                canonicalCodecName(audio.codecName),
                static_cast<std::uint8_t>(*audio.payloadType),
                *audio.clockRate}};
        plan.streams = MediaRawRtpProbePlan::AudioVideo{
            std::move(videoProbeStream), std::move(audioProbeStream)};
    } else {
        plan.streams = MediaRawRtpProbePlan::VideoOnly{
            std::move(videoProbeStream)};
    }
    plan.openTimeoutMs = *request.input.openTimeoutMs;
    plan.analyzeDurationUs = *request.input.analyzeDurationUs;
    plan.maximumBufferedBytes =
        static_cast<std::size_t>(*request.input.probeSizeBytes);
    plan.reorderWindowPackets = RtpReorderWindowPackets;
    plan.maximumReorderDelayMs = RtpMaximumReorderDelayMs;
    const std::string codec = canonicalCodecName(metadata.codecName);
    if (codec == "h264") {
        plan.packetizationPolicy =
            MediaRtpVideoPacketizationPolicy::H264NonInterleaved;
    } else if (codec == "hevc") {
        plan.packetizationPolicy =
            MediaRtpVideoPacketizationPolicy::HevcNonInterleavedNoDonl;
    } else {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            ::media::ErrorInfo::unsupported(
                "raw RTP video auto-detection supports only H264 and HEVC"));
    }
    plan.rtcpPolicy = MediaRtcpCompoundPolicy{
        MediaRtcpCompositionMode::ReducedSizeRfc5506, false};
    return MediaRawRtpInputPreparer::prepare(plan);
}

} // namespace media::ffmpeg::graph

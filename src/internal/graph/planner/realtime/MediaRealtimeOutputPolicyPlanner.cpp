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

::media::Result<int> plannedRtpSendBufferBytes(
    int maximumDatagramBytes,
    std::uint64_t maximumAccessUnitBytes)
{
    if (maximumDatagramBytes <= 0 ||
        maximumAccessUnitBytes == 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP sender buffer requires positive datagram and access-unit facts"));
    }
    const std::uint64_t selected = (std::max)(
        maximumAccessUnitBytes,
        static_cast<std::uint64_t>(maximumDatagramBytes));
    if (selected > static_cast<std::uint64_t>(
                       std::numeric_limits<int>::max())) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP sender buffer exceeds the socket option range"));
    }
    return ::media::Result<int>::success(static_cast<int>(selected));
}

void applyPacing(
    MediaRealtimeScheduledRtpOutputPlanningDraft& output,
    int64_t bitsPerSecond) noexcept
{
    output.writePacingEnabled = true;
    output.writePacingBytesPerSecond = pacingBytesPerSecond(bitsPerSecond);
    output.writePacingBurstBytes = std::max<int64_t>(1, static_cast<int64_t>(output.packetSize) * PacingBurstPackets);
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

::media::Result<MediaRtpUdpSenderConfig> rtpTransport(
    const std::string& host,
    std::size_t rtpPort,
    int maximumDatagramBytes,
    int sendBufferBytes)
{
    const bool bracketedIpv6 = host.size() > 2 && host.front() == '[' &&
        host.back() == ']';
    const std::string numericHost = bracketedIpv6
        ? host.substr(1, host.size() - 2)
        : host;
    if (!validRtpPort(rtpPort) || maximumDatagramBytes <= 0 ||
        sendBufferBytes <= 0) {
        return ::media::Result<MediaRtpUdpSenderConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP transport facts are incomplete"));
    }
    return MediaRtpUdpSenderConfig::create(
        bracketedIpv6 ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
        bracketedIpv6 ? "::" : "0.0.0.0",
        numericHost,
        static_cast<std::uint16_t>(rtpPort),
        static_cast<std::uint16_t>(rtpPort + 1),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        sendBufferBytes,
        static_cast<std::size_t>(maximumDatagramBytes),
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
    if (request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        output.muxedOutput.url = urls.muxed;
        output.muxedOutput.mediaId = request.mediaId;
        const bool expectAudio =
            request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo;
        if (!request.output.transport) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS output requires a resolved transport"));
        }
        if (MediaRealtimeRequestClassifier::rtpAvpOutput(request)) {
            if (!request.output.basePort || !request.output.packetSize ||
                !request.output.pacingBitrateBps ||
                *request.output.pacingBitrateBps < 8) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS RTP output requires explicit endpoint and datagram facts"));
            }
            if (!request.avSyncStartup.maximumVideoUnitBytes ||
                *request.avSyncStartup.maximumVideoUnitBytes == 0 ||
                (expectAudio &&
                 (!request.avSyncStartup.maximumAudioUnitBytes ||
                  *request.avSyncStartup.maximumAudioUnitBytes == 0))) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS RTP sender requires planner-owned access-unit bounds"));
            }
            auto sendBuffer = plannedRtpSendBufferBytes(
                *request.output.packetSize,
                std::max(
                    static_cast<std::uint64_t>(
                        *request.avSyncStartup.maximumVideoUnitBytes),
                    expectAudio
                        ? static_cast<std::uint64_t>(
                              *request.avSyncStartup.maximumAudioUnitBytes)
                        : std::uint64_t{0}));
            if (!sendBuffer) {
                return ::media::Status::failure(sendBuffer.error());
            }
            auto transport = rtpTransport(
                request.output.host, *request.output.basePort,
                *request.output.packetSize,
                sendBuffer.value());
            if (!transport) {
                return ::media::Status::failure(transport.error());
            }
            output.muxedOutput.rtpTransport =
                std::move(transport).value();
            output.muxedOutput.sdpPath = request.output.sdpPath;
            output.muxedOutput.scheduledWireBytesPerSecond =
                *request.output.pacingBitrateBps / 8;
        }
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
    if (output.videoOutput.writePacingBurstBytes >
            std::numeric_limits<int>::max()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP video transport send buffer exceeds integer range"));
    }
    auto videoTransport = rtpTransport(
        request.output.host, *request.output.basePort,
        output.videoOutput.packetSize,
        static_cast<int>(output.videoOutput.writePacingBurstBytes));
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
        const int audioBitrateKbps = *audioBitrate;
        output.audioOutput.url = urls.audio;
        output.audioOutput.packetSize = *request.output.packetSize;
        output.audioOutput.mediaId = request.mediaId;
        applyPacing(output.audioOutput,
                    static_cast<int64_t>(audioBitrateKbps) * 1000);
        if (output.audioOutput.writePacingBurstBytes >
                std::numeric_limits<int>::max()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "scheduled RTP transport send buffer exceeds integer range"));
        }
        auto audioTransport = rtpTransport(
            request.output.host, *request.output.basePort + 2,
            output.audioOutput.packetSize,
            static_cast<int>(output.audioOutput.writePacingBurstBytes));
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

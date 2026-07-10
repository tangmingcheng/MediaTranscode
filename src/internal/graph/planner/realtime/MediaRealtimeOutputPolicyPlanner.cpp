#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RtpSessionStartupDelayMs = 1000;
constexpr int64_t DefaultAudioBitsPerSecond = 320000;
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

} // namespace

::media::Result<MediaRealtimeOutputUrls> MediaRealtimeOutputPolicyPlanner::planUrls(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    MediaRealtimeOutputUrls urls;
    if (request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        if (request.output.url.empty()) {
            return ::media::Result<MediaRealtimeOutputUrls>::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS muxed output requires explicit output URL"));
        }
        urls.video = request.output.url;
        urls.muxed = request.output.url;
        urls.muxedFormat = "mpegts";
        return ::media::Result<MediaRealtimeOutputUrls>::success(std::move(urls));
    }

    if (!request.output.url.empty()) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP separate output requires host/basePort; single output URL is unsupported"));
    }
    if (request.output.host.empty() || !request.output.basePort) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output host and base port are required"));
    }
    const std::size_t videoPort = *request.output.basePort;
    const std::size_t audioPort = videoPort + 2;
    if (!validRtpPort(videoPort) || (MediaRealtimeRequestClassifier::audioRequested(request) && !validRtpPort(audioPort))) {
        return ::media::Result<MediaRealtimeOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output ports must be valid even RTP ports"));
    }
    urls.video = rtpUrl(request.output.host, videoPort);
    if (MediaRealtimeRequestClassifier::audioRequested(request)) urls.audio = rtpUrl(request.output.host, audioPort);
    return ::media::Result<MediaRealtimeOutputUrls>::success(std::move(urls));
}

::media::Status MediaRealtimeOutputPolicyPlanner::apply(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeOutputUrls& urls,
    MediaRealtimeRtpTranscodePlan& plan)
{
    if (request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        plan.muxedOutput.url = urls.muxed;
        plan.muxedOutput.format = urls.muxedFormat;
        plan.muxedOutput.mediaId = request.mediaId;
        plan.videoMux.expectVideo = true;
        plan.videoMux.expectAudio = MediaRealtimeRequestClassifier::audioRequested(request);
        return ::media::Status::success();
    }

    if (!plan.videoParameters.bitrateKbps || !request.output.packetSize) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime output policy requires resolved video bitrate and packet size"));
    }
    const std::string codec = canonicalCodecName(plan.videoPlan.outputCodecName);
    if (codec == "h264" || codec == "avc" || codec == "avc1") plan.videoParameters.globalHeader = true;

    plan.videoOutput.url = urls.video;
    plan.videoOutput.packetSize = *request.output.packetSize;
    plan.videoOutput.mediaId = request.mediaId;
    applyPacing(plan.videoOutput, static_cast<int64_t>(*plan.videoParameters.bitrateKbps) * 1000);
    plan.audioOutput.url = urls.audio;
    plan.audioOutput.packetSize = *request.output.packetSize;
    plan.audioOutput.mediaId = request.mediaId;
    applyPacing(plan.audioOutput, request.parameters.audio.bitrateKbps
                                      ? static_cast<int64_t>(*request.parameters.audio.bitrateKbps) * 1000
                                      : DefaultAudioBitsPerSecond);
    plan.sdp.path = request.output.sdpPath;
    plan.sdp.mediaId = request.mediaId;
    plan.sdp.expectedContexts = MediaRealtimeRequestClassifier::audioRequested(request) ? 2 : 1;
    plan.videoMux.expectVideo = true;
    plan.videoMux.pacingPolicy = muxPacing();
    plan.videoMux.monotonicPacketTimestamps = true;
    plan.videoMux.startupDelayMs = RtpSessionStartupDelayMs;
    plan.audioMux.expectAudio = MediaRealtimeRequestClassifier::audioRequested(request);
    plan.audioMux.pacingPolicy = muxPacing();
    plan.audioMux.monotonicPacketTimestamps = MediaRealtimeRequestClassifier::audioRequested(request);
    plan.audioMux.startupDelayMs = MediaRealtimeRequestClassifier::audioRequested(request) ? RtpSessionStartupDelayMs : 0;
    plan.avStartBarrier.expectVideo = true;
    plan.avStartBarrier.expectAudio = MediaRealtimeRequestClassifier::audioRequested(request);
    plan.avStartBarrier.requireVideoKeyFrame = MediaRealtimeRequestClassifier::audioRequested(request);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

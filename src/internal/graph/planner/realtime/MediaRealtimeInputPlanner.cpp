#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RawVideoStreamIndex = 0;
constexpr int RawAudioStreamIndex = 0;

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
    return parsed;
}

std::string sdp(
    const MediaRtpUrlEndpoint& endpoint,
    const MediaRealtimeRtpInputMetadata& metadata,
    const MediaRealtimeRtpCodecDescriptor& descriptor,
    const std::string& mediaId)
{
    std::ostringstream out;
    out << "v=0\r\n"
        << "o=- 0 0 IN IP4 127.0.0.1\r\n"
        << "s=MediaTranscode Raw RTP\r\n"
        << "t=0 0\r\n";
    const char* mediaName = descriptor.streamKind == MediaStreamKind::Audio ? "audio" : "video";
    out << "m=" << mediaName << " " << endpoint.port << " RTP/AVP " << *metadata.payloadType << "\r\n"
        << "c=IN IP4 " << endpoint.host << "\r\n"
        << "a=rtpmap:" << *metadata.payloadType << " " << descriptor.rtpEncodingName << "/"
        << descriptor.clockRate;
    if (descriptor.channels > 0) out << "/" << descriptor.channels;
    out << "\r\n";
    if (!metadata.fmtp.empty()) out << "a=fmtp:" << *metadata.payloadType << " " << metadata.fmtp << "\r\n";
    if (!mediaId.empty()) out << "a=control:" << mediaId << "." << mediaName << "\r\n";
    return out.str();
}

void fillNodePlan(
    const MediaRealtimeRtpTranscodeRequest& request,
    std::string url,
    std::string sdpText,
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
}

} // namespace

::media::Result<MediaRealtimeRawInputPlan> MediaRealtimeInputPlanner::planRawRtp(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    auto videoDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Video, request.input.videoRtp);
    if (!videoDescriptor) return ::media::Result<MediaRealtimeRawInputPlan>::failure(videoDescriptor.error());
    auto videoEndpoint = endpoint(request.input.videoRtp, "Raw RTP video");
    if (!videoEndpoint) return ::media::Result<MediaRealtimeRawInputPlan>::failure(videoEndpoint.error());

    MediaRealtimeRawInputPlan result;
    result.videoUrl = request.input.videoRtp.url;
    result.videoSdp = sdp(videoEndpoint.value(), request.input.videoRtp, videoDescriptor.value(), request.mediaId);
    result.video.streamIndex = RawVideoStreamIndex;
    result.video.codecName = videoDescriptor.value().codecName;

    if (MediaRealtimeRequestClassifier::audioRequested(request)) {
        auto audioDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Audio, request.input.audioRtp);
        if (!audioDescriptor) return ::media::Result<MediaRealtimeRawInputPlan>::failure(audioDescriptor.error());
        auto audioEndpoint = endpoint(request.input.audioRtp, "Raw RTP audio");
        if (!audioEndpoint) return ::media::Result<MediaRealtimeRawInputPlan>::failure(audioEndpoint.error());
        result.audioUrl = request.input.audioRtp.url;
        result.audioSdp = sdp(audioEndpoint.value(), request.input.audioRtp, audioDescriptor.value(), request.mediaId);
        MediaInputAudioStreamInfo audio;
        audio.streamIndex = RawAudioStreamIndex;
        audio.codecName = audioDescriptor.value().codecName;
        audio.sampleRate = audioDescriptor.value().clockRate;
        audio.channels = audioDescriptor.value().channels;
        result.audio = std::move(audio);
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
                 plan.input);
    if (raw && raw->audio) {
        plan.useIsolatedAudioInput = true;
        fillNodePlan(request, raw->audioUrl, raw->audioSdp, plan.audioInput);
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

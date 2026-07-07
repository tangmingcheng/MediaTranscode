#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <sstream>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RealtimeNoBidirectionalFrames = 0;
constexpr bool RealtimeRequiresFilterGraph = true;
constexpr bool RealtimeRequiresRuntimeAvailability = true;
constexpr int H264RtpClockRate = 90000;
constexpr int RawRtpVideoStreamIndex = 0;

bool isValidRtpPort(std::size_t port) noexcept
{
    return port > 0 && port <= 65534 && (port % 2) == 0;
}

bool planGpuPreference(const MediaTranscodeExecutionParameters& execution) noexcept
{
    return !execution.disableHardware;
}

bool planSoftwareChain(const MediaTranscodeExecutionParameters& execution) noexcept
{
    return execution.disableHardware;
}

std::string planPreferredHardware(const MediaTranscodeExecutionParameters& execution)
{
    return planGpuPreference(execution) ? "auto" : "software";
}

MediaVideoTranscodeParameters planRealtimeVideoParameters(const MediaVideoTranscodeParameters& requested)
{
    MediaVideoTranscodeParameters planned = requested;
    planned.bFrames = RealtimeNoBidirectionalFrames;
    return planned;
}

::media::Result<std::string> planOutputUrl(const MediaRealtimeOutputConfig& output)
{
    if (!output.url.empty()) {
        return ::media::Result<std::string>::success(output.url);
    }
    if (output.host.empty() || !output.basePort.has_value()) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output requires explicit url or host/basePort"));
    }
    if (!isValidRtpPort(*output.basePort)) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument("RTP output base port must be an even port in range 1..65534"));
    }
    return ::media::Result<std::string>::success(
        "rtp://" + output.host + ":" + std::to_string(*output.basePort));
}

::media::Result<void> validateRealtimeUrlInput(const MediaRealtimeRtpTranscodeRequest& options)
{
    if (isUnsupportedRealtimeInputUrl(options.input.url)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("Realtime URL input does not accept raw RTP, UDP, or SDP URLs"));
    }
    if (options.input.rtspTransport.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime URL input requires explicit RTSP transport"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateRealtimeInputOpenOptions(const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.openTimeoutMs.has_value() ||
        !options.input.readTimeoutMs.has_value() ||
        !options.input.analyzeDurationUs.has_value() ||
        !options.input.probeSizeBytes.has_value() ||
        !options.input.lowLatency.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input requires explicit timeouts, probe, and latency options"));
    }
    return ::media::Result<void>::success();
}

::media::Result<MediaRtpUrlEndpoint> validateRawRtpInput(const MediaRealtimeRtpTranscodeRequest& options)
{
    if (options.input.rtp.codecName.empty() ||
        !options.input.rtp.payloadType.has_value() ||
        !options.input.rtp.clockRate.has_value()) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input requires codec name, payload type, and clock rate"));
    }
    if (canonicalCodecName(options.input.rtp.codecName) != "h264") {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input currently supports H264 only"));
    }
    if (*options.input.rtp.payloadType < 96 || *options.input.rtp.payloadType > 127) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP H264 payload type must be dynamic range 96..127"));
    }
    if (*options.input.rtp.clockRate != H264RtpClockRate) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP H264 clock rate must be 90000"));
    }

    auto endpoint = parseRtpUdpUrlEndpoint(options.input.url);
    if (!endpoint) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(endpoint.error());
    }
    return endpoint;
}

std::string planRawH264Sdp(const MediaRtpUrlEndpoint& endpoint,
                           int payloadType,
                           const std::string& mediaId)
{
    std::ostringstream out;
    out << "v=0\r\n"
        << "o=- 0 0 IN IP4 " << endpoint.host << "\r\n"
        << "s=MediaTranscode Raw H264 RTP\r\n"
        << "c=IN IP4 " << endpoint.host << "\r\n"
        << "t=0 0\r\n"
        << "m=video " << endpoint.port << " RTP/AVP " << payloadType << "\r\n"
        << "a=rtpmap:" << payloadType << " H264/90000\r\n";
    if (!mediaId.empty()) {
        out << "a=control:" << mediaId << "\r\n";
    }
    return out.str();
}

::media::Result<MediaPipelinePlannerOptions> planVideoPipelineOptions(
    const MediaRealtimeRtpTranscodeRequest& options,
    const std::string& outputUrl)
{
    const MediaVideoTranscodeParameters& video = options.parameters.video;
    if (video.width.has_value() != video.height.has_value()) {
        return ::media::Result<MediaPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP video width and height must be specified together"));
    }
    if (video.codecName.empty()) {
        return ::media::Result<MediaPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP video codec must be explicit"));
    }

    MediaPipelinePlannerOptions plannerOptions;
    plannerOptions.includeVideo = options.parameters.execution.includeVideo;
    plannerOptions.allowPacketCopy = false;
    plannerOptions.outputPath = outputUrl;
    plannerOptions.outputCodecName = video.codecName;
    plannerOptions.targetWidth = video.width.value_or(0);
    plannerOptions.targetHeight = video.height.value_or(0);
    plannerOptions.filterRequired = RealtimeRequiresFilterGraph;
    plannerOptions.preferGpu = planGpuPreference(options.parameters.execution);
    plannerOptions.enableSoftwareChain = planSoftwareChain(options.parameters.execution);
    plannerOptions.requireRuntimeAvailability = RealtimeRequiresRuntimeAvailability;
    plannerOptions.preferredHardware = planPreferredHardware(options.parameters.execution);
    plannerOptions.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    plannerOptions.rtspTransport = options.input.rtspTransport;
    plannerOptions.openTimeoutMs = *options.input.openTimeoutMs;
    plannerOptions.readTimeoutMs = *options.input.readTimeoutMs;
    plannerOptions.analyzeDurationUs = *options.input.analyzeDurationUs;
    plannerOptions.probeSizeBytes = *options.input.probeSizeBytes;
    plannerOptions.lowLatency = *options.input.lowLatency;
    return ::media::Result<MediaPipelinePlannerOptions>::success(std::move(plannerOptions));
}

::media::Result<void> validateQueues(const MediaGraphQueueParameters& queues)
{
    if (queues.metadata == 0 || queues.packet == 0 || queues.frame == 0 || queues.mux == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP queue capacities must be greater than 0"));
    }
    return ::media::Result<void>::success();
}

MediaEdgePolicy planEdgePolicy(const MediaGraphQueueParameters& queues)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::SpscRing;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;
    policy.queuePolicy.orderingPolicy = MediaQueueOrderingPolicy::Timestamp;
    policy.queuePolicy.capacity = queues.packet;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.collectMetrics = true;
    policy.queuePolicy.backpressurePolicy.mode = MediaBackpressureMode::Adaptive;
    policy.queuePolicy.backpressurePolicy.lowWatermark = queues.packet / 2;
    policy.queuePolicy.backpressurePolicy.highWatermark = queues.packet - 1;
    policy.queuePolicy.backpressurePolicy.criticalWatermark = queues.packet;
    policy.queuePolicy.backpressurePolicy.realtimePriority = true;
    policy.backpressurePolicy = policy.queuePolicy.backpressurePolicy;
    return policy;
}

} // namespace

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.kind.has_value()) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input kind must be explicit"));
    }
    if (options.input.url.empty()) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input URL must be explicit"));
    }
    if (!options.parameters.execution.includeVideo) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP transcode requires video branch"));
    }
    if (!options.output.packetSize.has_value() || *options.output.packetSize <= 0) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output packet size must be explicit and positive"));
    }
    if (options.output.sdpPath.empty()) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP SDP output path must be explicit"));
    }
    if (auto queueStatus = validateQueues(options.parameters.queues); !queueStatus) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(queueStatus.error());
    }

    switch (*options.input.kind) {
    case MediaRealtimeInputKind::RealtimeUrl:
        if (auto status = validateRealtimeUrlInput(options); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
        }
        break;
    case MediaRealtimeInputKind::RawRtp:
        break;
    }

    if (auto status = validateRealtimeInputOpenOptions(options); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }

    auto outputUrl = planOutputUrl(options.output);
    if (!outputUrl) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputUrl.error());
    }

    auto pipelineOptionsResult = planVideoPipelineOptions(options, outputUrl.value());
    if (!pipelineOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(pipelineOptionsResult.error());
    }
    MediaPipelinePlannerOptions pipelineOptions = std::move(pipelineOptionsResult).value();

    std::string rawRtpSdpText;
    ::media::Result<MediaPipelinePlan> plannedVideo =
        ::media::Result<MediaPipelinePlan>::failure(::media::ErrorInfo::internalError("unplanned realtime RTP video"));

    if (*options.input.kind == MediaRealtimeInputKind::RawRtp) {
        auto endpoint = validateRawRtpInput(options);
        if (!endpoint) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(endpoint.error());
        }

        MediaInputVideoStreamInfo inputInfo;
        inputInfo.streamIndex = RawRtpVideoStreamIndex;
        inputInfo.codecName = "h264";
        rawRtpSdpText = planRawH264Sdp(endpoint.value(), *options.input.rtp.payloadType, options.mediaId);
        plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
            std::move(inputInfo),
            options.input.url,
            std::move(pipelineOptions));
    } else {
        plannedVideo = MediaPipelinePlanner::planVideoTranscodeRealtimeUrl(
            options.input.url,
            std::move(pipelineOptions));
    }
    if (!plannedVideo) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideo.error());
    }

    MediaRealtimeRtpTranscodePlan plan;
    plan.inputKind = *options.input.kind;
    plan.videoPlan = std::move(plannedVideo).value();
    plan.videoParameters = planRealtimeVideoParameters(options.parameters.video);
    plan.queues = options.parameters.queues;
    plan.edgePolicy = planEdgePolicy(options.parameters.queues);
    plan.input.url = options.input.url;
    plan.input.sdpText = std::move(rawRtpSdpText);
    plan.input.rtspTransport = options.input.rtspTransport;
    plan.input.openTimeoutMs = *options.input.openTimeoutMs;
    plan.input.readTimeoutMs = *options.input.readTimeoutMs;
    plan.input.analyzeDurationUs = *options.input.analyzeDurationUs;
    plan.input.probeSizeBytes = *options.input.probeSizeBytes;
    plan.input.lowLatency = *options.input.lowLatency;
    plan.input.mediaId = options.mediaId;
    plan.output.url = outputUrl.value();
    plan.output.packetSize = *options.output.packetSize;
    plan.output.mediaId = options.mediaId;
    plan.sdp.path = options.output.sdpPath;
    plan.sdp.mediaId = options.mediaId;
    plan.mux.expectVideo = true;
    plan.mux.expectAudio = false;
    return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph

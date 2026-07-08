#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <sstream>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RealtimeNoBidirectionalFrames = 0;
constexpr bool RealtimeRequiresFilterGraph = true;
constexpr bool RealtimeRequiresRuntimeAvailability = true;
constexpr int RawRtpVideoStreamIndex = 0;
constexpr int RawRtpAudioStreamIndex = 1;

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

struct PlannedOutputUrls {
    std::string videoUrl;
    std::string audioUrl;
};

bool audioRequested(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.parameters.execution.includeAudio;
}

::media::Result<PlannedOutputUrls> planOutputUrls(const MediaRealtimeOutputConfig& output,
                                                  bool includeAudio)
{
    if (!output.url.empty()) {
        if (includeAudio) {
            return ::media::Result<PlannedOutputUrls>::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP audio output requires host/basePort so planner can derive per-stream ports"));
        }
        return ::media::Result<PlannedOutputUrls>::success({ output.url, {} });
    }
    if (output.host.empty() || !output.basePort.has_value()) {
        return ::media::Result<PlannedOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP output requires explicit url or host/basePort"));
    }
    if (!isValidRtpPort(*output.basePort)) {
        return ::media::Result<PlannedOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("RTP output base port must be an even port in range 1..65534"));
    }
    if (includeAudio && *output.basePort > 65532) {
        return ::media::Result<PlannedOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("RTP output base port leaves no room for audio port"));
    }

    PlannedOutputUrls urls;
    urls.videoUrl = "rtp://" + output.host + ":" + std::to_string(*output.basePort);
    if (includeAudio) {
        urls.audioUrl = "rtp://" + output.host + ":" + std::to_string(*output.basePort + 2);
    }
    return ::media::Result<PlannedOutputUrls>::success(std::move(urls));
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

::media::Result<MediaRtpUrlEndpoint> validateRawRtpEndpoint(const MediaRealtimeRtpInputMetadata& metadata,
                                                            const std::string& owner)
{
    auto endpoint = parseRtpUdpUrlEndpoint(metadata.url);
    if (!endpoint) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(endpoint.error());
    }
    if (!isValidRtpPort(endpoint.value().port)) {
        return ::media::Result<MediaRtpUrlEndpoint>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " RTP URL requires an even port in range 1..65534"));
    }
    return endpoint;
}

void appendRawRtpSdpMedia(std::ostringstream& out,
                          const MediaRtpUrlEndpoint& endpoint,
                          const MediaRealtimeRtpInputMetadata& metadata,
                          const MediaRealtimeRtpCodecDescriptor& descriptor,
                          const std::string& mediaId)
{
    const char* mediaName = descriptor.streamKind == MediaStreamKind::Audio ? "audio" : "video";
    out << "m=" << mediaName << " " << endpoint.port << " RTP/AVP " << *metadata.payloadType << "\r\n"
        << "c=IN IP4 " << endpoint.host << "\r\n"
        << "a=rtpmap:" << *metadata.payloadType << " " << descriptor.rtpEncodingName << "/" << descriptor.clockRate;
    if (descriptor.streamKind == MediaStreamKind::Audio && descriptor.channels > 0) {
        out << "/" << descriptor.channels;
    }
    out << "\r\n";
    if (!metadata.fmtp.empty()) {
        out << "a=fmtp:" << *metadata.payloadType << " " << metadata.fmtp << "\r\n";
    }
    if (!mediaId.empty()) {
        out << "a=control:" << mediaId << "." << mediaName << "\r\n";
    }
}

std::string planRawRtpSdp(const MediaRtpUrlEndpoint& videoEndpoint,
                          const MediaRealtimeRtpInputMetadata& videoMetadata,
                          const MediaRealtimeRtpCodecDescriptor& videoDescriptor,
                          const MediaRtpUrlEndpoint* audioEndpoint,
                          const MediaRealtimeRtpInputMetadata* audioMetadata,
                          const MediaRealtimeRtpCodecDescriptor* audioDescriptor,
                          const std::string& mediaId)
{
    std::ostringstream out;
    out << "v=0\r\n"
        << "o=- 0 0 IN IP4 " << videoEndpoint.host << "\r\n"
        << "s=MediaTranscode Raw RTP\r\n"
        << "t=0 0\r\n";
    appendRawRtpSdpMedia(out, videoEndpoint, videoMetadata, videoDescriptor, mediaId);
    if (audioEndpoint && audioMetadata && audioDescriptor) {
        appendRawRtpSdpMedia(out, *audioEndpoint, *audioMetadata, *audioDescriptor, mediaId);
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

::media::Status validatePositiveOptional(const std::optional<int>& value, const char* name)
{
    if (value && *value <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(std::string(name) + " must be positive"));
    }
    return ::media::Status::success();
}

::media::Result<MediaAudioPipelinePlannerOptions> planAudioPipelineOptions(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    const MediaAudioTranscodeParameters& audio = options.parameters.audio;
    if (auto status = validatePositiveOptional(audio.bitrateKbps, "audio bitrate"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }
    if (auto status = validatePositiveOptional(audio.minBitrateKbps, "audio min bitrate"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }
    if (auto status = validatePositiveOptional(audio.maxBitrateKbps, "audio max bitrate"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }
    if (audio.minBitrateKbps && audio.maxBitrateKbps && *audio.minBitrateKbps > *audio.maxBitrateKbps) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::invalidArgument("audio min bitrate must be <= audio max bitrate"));
    }
    if (auto status = validatePositiveOptional(audio.bufferSizeKbits, "audio buffer size"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }
    if (auto status = validatePositiveOptional(audio.sampleRate, "audio sample rate"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }
    if (auto status = validatePositiveOptional(audio.channels, "audio channels"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }
    if (auto status = validatePositiveOptional(audio.quality, "audio quality"); !status) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(status.error());
    }

    MediaAudioPipelinePlannerOptions plannerOptions;
    plannerOptions.includeAudio = options.parameters.execution.includeAudio;
    plannerOptions.requestedCodecName = audio.codecName;
    plannerOptions.rateControl = audio.rateControl;
    plannerOptions.requestedBitrateKbps = audio.bitrateKbps;
    plannerOptions.requestedMinBitrateKbps = audio.minBitrateKbps;
    plannerOptions.requestedMaxBitrateKbps = audio.maxBitrateKbps;
    plannerOptions.requestedBufferSizeKbits = audio.bufferSizeKbits;
    plannerOptions.requestedSampleRate = audio.sampleRate;
    plannerOptions.requestedChannels = audio.channels;
    plannerOptions.requestedQuality = audio.quality;
    plannerOptions.requestedPreset = audio.preset;
    plannerOptions.requestedProfile = audio.profile;
    plannerOptions.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    return ::media::Result<MediaAudioPipelinePlannerOptions>::success(std::move(plannerOptions));
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
    if (*options.input.kind == MediaRealtimeInputKind::RealtimeUrl && options.input.url.empty()) {
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

    auto outputUrls = planOutputUrls(options.output, audioRequested(options));
    if (!outputUrls) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputUrls.error());
    }

    auto pipelineOptionsResult = planVideoPipelineOptions(options, outputUrls.value().videoUrl);
    if (!pipelineOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(pipelineOptionsResult.error());
    }
    MediaPipelinePlannerOptions pipelineOptions = std::move(pipelineOptionsResult).value();

    auto audioOptionsResult = planAudioPipelineOptions(options);
    if (!audioOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioOptionsResult.error());
    }
    MediaAudioPipelinePlannerOptions audioOptions = std::move(audioOptionsResult).value();

    std::string rawRtpSdpText;
    std::string plannedInputUrl = options.input.url;
    MediaPipelinePlan videoPlan;
    MediaAudioPipelinePlan audioPlan;
    if (*options.input.kind == MediaRealtimeInputKind::RawRtp) {
        auto videoDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Video, options.input.videoRtp);
        if (!videoDescriptor) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(videoDescriptor.error());
        }
        auto videoEndpoint = validateRawRtpEndpoint(options.input.videoRtp, "Raw RTP video");
        if (!videoEndpoint) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(videoEndpoint.error());
        }

        const MediaRtpUrlEndpoint* audioEndpointPtr = nullptr;
        const MediaRealtimeRtpInputMetadata* audioMetadataPtr = nullptr;
        const MediaRealtimeRtpCodecDescriptor* audioDescriptorPtr = nullptr;
        MediaRtpUrlEndpoint audioEndpointValue;
        MediaRealtimeRtpCodecDescriptor audioDescriptorValue;

        if (audioRequested(options)) {
            auto audioDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Audio, options.input.audioRtp);
            if (!audioDescriptor) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioDescriptor.error());
            }
            auto audioEndpoint = validateRawRtpEndpoint(options.input.audioRtp, "Raw RTP audio");
            if (!audioEndpoint) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioEndpoint.error());
            }
            audioEndpointValue = audioEndpoint.value();
            audioDescriptorValue = audioDescriptor.value();
            audioEndpointPtr = &audioEndpointValue;
            audioMetadataPtr = &options.input.audioRtp;
            audioDescriptorPtr = &audioDescriptorValue;

            MediaInputAudioStreamInfo audioInfo;
            audioInfo.streamIndex = RawRtpAudioStreamIndex;
            audioInfo.codecName = audioDescriptorValue.codecName;
            audioInfo.sampleRate = audioDescriptorValue.clockRate;
            audioInfo.channels = audioDescriptorValue.channels;
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudioTranscode(std::move(audioInfo), audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan = std::move(plannedAudio).value();
        } else {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudioTranscode({}, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan = std::move(plannedAudio).value();
        }

        rawRtpSdpText = planRawRtpSdp(videoEndpoint.value(),
                                      options.input.videoRtp,
                                      videoDescriptor.value(),
                                      audioEndpointPtr,
                                      audioMetadataPtr,
                                      audioDescriptorPtr,
                                      options.mediaId);
        plannedInputUrl = options.input.videoRtp.url;

        MediaInputVideoStreamInfo inputInfo;
        inputInfo.streamIndex = RawRtpVideoStreamIndex;
        inputInfo.codecName = videoDescriptor.value().codecName;
        auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
            std::move(inputInfo),
            plannedInputUrl,
            std::move(pipelineOptions));
        if (!plannedVideo) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideo.error());
        }
        videoPlan = std::move(plannedVideo).value();
    } else {
        auto realtimeInput = MediaPipelineCapabilityScanner::detectRealtimeInputStreamInfo(options.input.url,
                                                                                           pipelineOptions,
                                                                                           audioRequested(options));
        if (!realtimeInput) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(realtimeInput.error());
        }

        auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
            realtimeInput.value().video,
            options.input.url,
            std::move(pipelineOptions));
        if (!plannedVideo) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideo.error());
        }
        videoPlan = std::move(plannedVideo).value();

        if (audioRequested(options)) {
            if (!realtimeInput.value().hasAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::invalidArgument("Realtime RTP audio was requested but input has no audio stream"));
            }
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudioTranscode(realtimeInput.value().audio, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan = std::move(plannedAudio).value();
        } else {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudioTranscode({}, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan = std::move(plannedAudio).value();
        }
    }

    MediaRealtimeRtpTranscodePlan plan;
    plan.inputKind = *options.input.kind;
    plan.videoPlan = std::move(videoPlan);
    plan.audioPlan = std::move(audioPlan);
    plan.videoParameters = planRealtimeVideoParameters(options.parameters.video);
    plan.audioParameters = options.parameters.audio;
    plan.queues = options.parameters.queues;
    plan.edgePolicy = planEdgePolicy(options.parameters.queues);
    plan.input.url = plannedInputUrl;
    plan.input.sdpText = std::move(rawRtpSdpText);
    plan.input.rtspTransport = options.input.rtspTransport;
    plan.input.openTimeoutMs = *options.input.openTimeoutMs;
    plan.input.readTimeoutMs = *options.input.readTimeoutMs;
    plan.input.analyzeDurationUs = *options.input.analyzeDurationUs;
    plan.input.probeSizeBytes = *options.input.probeSizeBytes;
    plan.input.lowLatency = *options.input.lowLatency;
    plan.input.mediaId = options.mediaId;
    plan.videoOutput.url = outputUrls.value().videoUrl;
    plan.videoOutput.packetSize = *options.output.packetSize;
    plan.videoOutput.mediaId = options.mediaId;
    plan.audioOutput.url = outputUrls.value().audioUrl;
    plan.audioOutput.packetSize = *options.output.packetSize;
    plan.audioOutput.mediaId = options.mediaId;
    plan.sdp.path = options.output.sdpPath;
    plan.sdp.mediaId = options.mediaId;
    plan.sdp.expectedContexts = audioRequested(options) ? 2 : 1;
    plan.videoMux.expectVideo = true;
    plan.videoMux.expectAudio = false;
    plan.audioMux.expectVideo = false;
    plan.audioMux.expectAudio = audioRequested(options);
    return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <limits>
#include <sstream>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RealtimeNoBidirectionalFrames = 0;
constexpr int RealtimeDefaultGopFrames = 30;
constexpr int RealtimeRtpSessionStartupDelayMs = 1000;
constexpr int64_t RealtimeDefaultAudioOutputBitsPerSecond = 320000;
constexpr int64_t RealtimeOutputPacingHeadroomNumerator = 5;
constexpr int64_t RealtimeOutputPacingHeadroomDenominator = 4;
constexpr int64_t RealtimeOutputPacingBurstPackets = 2;
constexpr bool RealtimeRequiresRuntimeAvailability = true;
constexpr int RawRtpVideoStreamIndex = 0;
constexpr int RawRtpAudioStreamIndex = 0;

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
    if (!planned.gop) {
        planned.gop = RealtimeDefaultGopFrames;
    }
    planned.bFrames = RealtimeNoBidirectionalFrames;
    return planned;
}

int64_t planOutputWritePacingBytesPerSecond(int64_t bitsPerSecond) noexcept
{
    if (bitsPerSecond <= 0) {
        return 1;
    }
    const int64_t bytesPerSecond = (bitsPerSecond + 7) / 8;
    return std::max<int64_t>(1,
                             bytesPerSecond * RealtimeOutputPacingHeadroomNumerator /
                                 RealtimeOutputPacingHeadroomDenominator);
}

void applyRtpOutputWritePacing(MediaRealtimeRtpOutputNodePlan& output,
                               int64_t bitsPerSecond) noexcept
{
    output.writePacingEnabled = true;
    output.writePacingBytesPerSecond = planOutputWritePacingBytesPerSecond(bitsPerSecond);
    output.writePacingBurstBytes = std::max<int64_t>(1,
                                                     static_cast<int64_t>(output.packetSize) *
                                                         RealtimeOutputPacingBurstPackets);
}

::media::Result<MediaVideoTranscodeParameters> resolveRealtimeVideoParameters(
    const MediaVideoTranscodeParameters& requested,
    const MediaInputVideoStreamInfo& inputInfo)
{
    MediaVideoTranscodeParameters planned = planRealtimeVideoParameters(requested);
    if (planned.bitrateKbps && *planned.bitrateKbps < 0) {
        return ::media::Result<MediaVideoTranscodeParameters>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP video bitrate must be non-negative"));
    }
    if (!planned.bitrateKbps) {
        if (inputInfo.bitrateBitsPerSecond <= 0) {
            return ::media::Result<MediaVideoTranscodeParameters>::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP video bitrate is required when input bitrate is not observable"));
        }
        const int64_t sourceBitrateKbps = (inputInfo.bitrateBitsPerSecond + 999) / 1000;
        if (sourceBitrateKbps > std::numeric_limits<int>::max()) {
            return ::media::Result<MediaVideoTranscodeParameters>::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP video bitrate is too large"));
        }
        planned.bitrateKbps = static_cast<int>(sourceBitrateKbps);
    }
    if (planned.bitrateKbps && *planned.bitrateKbps == 0) {
        return ::media::Result<MediaVideoTranscodeParameters>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP video bitrate must be positive"));
    }
    return ::media::Result<MediaVideoTranscodeParameters>::success(std::move(planned));
}

struct PlannedOutputUrls {
    std::string videoUrl;
    std::string audioUrl;
    std::string muxedUrl;
    std::string muxedFormat;
};

std::string planOutputRtpUrl(const std::string& host, std::size_t port)
{
    return "rtp://" + host + ":" + std::to_string(port) + "?localrtpport=0&localrtcpport=0";
}

bool audioRequested(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.parameters.execution.includeAudio;
}

bool isRealtimeUrlInput(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.input.type == RealtimeInputType::Url &&
           options.input.streamLayout == RealtimeInputStreamLayout::SessionDescribed;
}

bool isRtpPortInput(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.input.type == RealtimeInputType::RtpPort &&
           options.input.streamLayout == RealtimeInputStreamLayout::SeparateStreams;
}

bool isMpegTsUdpInput(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.input.type == RealtimeInputType::MpegTsUdp &&
           options.input.streamLayout == RealtimeInputStreamLayout::MuxedTransportStream;
}

bool isSeparateRtpOutput(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.output.streamLayout == RealtimeOutputStreamLayout::SeparateStreams;
}

bool isMuxedTransportStreamOutput(const MediaRealtimeRtpTranscodeRequest& options) noexcept
{
    return options.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream;
}

bool rtpVideoCodecRequiresGlobalHeader(const std::string& codecName)
{
    const std::string codec = canonicalCodecName(codecName);
    return codec == "h264" || codec == "avc" || codec == "avc1";
}

void applyRealtimeOutputVideoRequirements(MediaVideoTranscodeParameters& parameters,
                                          RealtimeOutputStreamLayout outputLayout,
                                          const std::string& outputCodecName)
{
    if (outputLayout == RealtimeOutputStreamLayout::SeparateStreams &&
        rtpVideoCodecRequiresGlobalHeader(outputCodecName)) {
        parameters.globalHeader = true;
    }
}

::media::Result<void> validateExplicitStreamClassification(const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.type.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime input type must be explicit"));
    }
    if (!options.input.streamLayout.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime input stream layout must be explicit"));
    }
    if (!options.output.streamLayout.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime output stream layout must be explicit"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateSupportedStreamClassification(const MediaRealtimeRtpTranscodeRequest& options)
{
    if (isRealtimeUrlInput(options) && isSeparateRtpOutput(options)) {
        return ::media::Result<void>::success();
    }
    if (isRtpPortInput(options) && isSeparateRtpOutput(options)) {
        return ::media::Result<void>::success();
    }
    if (isMpegTsUdpInput(options) && isMuxedTransportStreamOutput(options)) {
        return ::media::Result<void>::success();
    }
    return ::media::Result<void>::failure(
        ::media::ErrorInfo::unsupported("Realtime input type, input layout, and output layout combination is not supported"));
}

::media::Result<PlannedOutputUrls> planOutputUrls(const MediaRealtimeOutputConfig& output,
                                                  bool includeAudio)
{
    if (!output.url.empty()) {
        return ::media::Result<PlannedOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP separate output requires host/basePort; single output URL is unsupported"));
    }
    if (output.host.empty() || !output.basePort.has_value()) {
        return ::media::Result<PlannedOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP separate output requires host/basePort"));
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
    urls.videoUrl = planOutputRtpUrl(output.host, *output.basePort);
    if (includeAudio) {
        urls.audioUrl = planOutputRtpUrl(output.host, *output.basePort + 2);
    }
    return ::media::Result<PlannedOutputUrls>::success(std::move(urls));
}

::media::Result<PlannedOutputUrls> planMuxedTransportStreamOutput(const MediaRealtimeOutputConfig& output)
{
    if (output.url.empty()) {
        return ::media::Result<PlannedOutputUrls>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS muxed output requires explicit output URL"));
    }
    PlannedOutputUrls urls;
    urls.videoUrl = output.url;
    urls.muxedUrl = output.url;
    urls.muxedFormat = "mpegts";
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

::media::Result<void> validateMpegTsUdpInput(const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!isUdpUrl(options.input.url)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS UDP input requires udp:// URL"));
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

std::string planSingleRawRtpSdp(const MediaRtpUrlEndpoint& endpoint,
                                const MediaRealtimeRtpInputMetadata& metadata,
                                const MediaRealtimeRtpCodecDescriptor& descriptor,
                                const std::string& mediaId)
{
    std::ostringstream out;
    out << "v=0\r\n"
        << "o=- 0 0 IN IP4 127.0.0.1\r\n"
        << "s=MediaTranscode Raw RTP\r\n"
        << "t=0 0\r\n";
    appendRawRtpSdpMedia(out, endpoint, metadata, descriptor, mediaId);
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
    MediaPipelinePlannerOptions plannerOptions(false,
                                               video.resizeRequested(),
                                               planGpuPreference(options.parameters.execution),
                                               planSoftwareChain(options.parameters.execution),
                                               RealtimeRequiresRuntimeAvailability,
                                               *options.input.lowLatency);
    plannerOptions.outputPath = outputUrl;
    plannerOptions.outputCodecName = video.codecName;
    plannerOptions.targetWidth = video.width.value_or(0);
    plannerOptions.targetHeight = video.height.value_or(0);
    plannerOptions.preferredHardware = planPreferredHardware(options.parameters.execution);
    plannerOptions.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    plannerOptions.rtspTransport = options.input.rtspTransport;
    plannerOptions.openTimeoutMs = *options.input.openTimeoutMs;
    plannerOptions.readTimeoutMs = *options.input.readTimeoutMs;
    plannerOptions.analyzeDurationUs = *options.input.analyzeDurationUs;
    plannerOptions.probeSizeBytes = *options.input.probeSizeBytes;
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

    MediaAudioPipelinePlannerOptions plannerOptions(options.parameters.execution.includeAudio);
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

MediaEdgePolicy planRealtimeQueuePolicy(std::size_t capacity,
                                        MediaQueueOverflowPolicy overflowPolicy,
                                        MediaQueueOrderingPolicy orderingPolicy = MediaQueueOrderingPolicy::Timestamp)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::SpscRing;
    policy.queuePolicy.overflowPolicy = overflowPolicy;
    policy.queuePolicy.orderingPolicy = orderingPolicy;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.collectMetrics = true;
    policy.queuePolicy.backpressurePolicy.mode = MediaBackpressureMode::Adaptive;
    policy.queuePolicy.backpressurePolicy.lowWatermark = capacity / 2;
    policy.queuePolicy.backpressurePolicy.highWatermark = capacity > 0 ? capacity - 1 : 0;
    policy.queuePolicy.backpressurePolicy.criticalWatermark = capacity;
    policy.queuePolicy.backpressurePolicy.realtimePriority = true;
    policy.backpressurePolicy = policy.queuePolicy.backpressurePolicy;
    return policy;
}

MediaRealtimeEdgePolicySet planEdgePolicies(const MediaGraphQueueParameters& queues)
{
    MediaRealtimeEdgePolicySet policies;
    policies.metadata = planRealtimeQueuePolicy(queues.metadata,
                                               MediaQueueOverflowPolicy::BlockProducer,
                                               MediaQueueOrderingPolicy::Fifo);
    policies.packet = planRealtimeQueuePolicy(queues.packet, MediaQueueOverflowPolicy::DropOldest);
    policies.videoPacket = planRealtimeQueuePolicy(queues.packet, MediaQueueOverflowPolicy::DropNonKeyFrame);
    policies.audioPacket = planRealtimeQueuePolicy(queues.packet, MediaQueueOverflowPolicy::DropOldest);
    policies.frame = planRealtimeQueuePolicy(queues.frame, MediaQueueOverflowPolicy::DropOldest);
    policies.mux = planRealtimeQueuePolicy(queues.mux, MediaQueueOverflowPolicy::DropOldest);
    policies.videoMux = planRealtimeQueuePolicy(queues.mux, MediaQueueOverflowPolicy::DropNonKeyFrame);
    policies.audioMux = planRealtimeQueuePolicy(queues.mux, MediaQueueOverflowPolicy::DropOldest);
    return policies;
}

MediaThreadingPolicy planThreadingPolicy() noexcept
{
    MediaThreadingPolicy policy;
    policy.mode = MediaThreadingMode::PerNodeWorker;
    policy.priority = MediaThreadPriority::High;
    policy.idleSleepMs = 0;
    policy.maxIdleSpins = 1;
    policy.collectWorkerMetrics = true;
    return policy;
}

MediaLatencyPolicy planRtpMuxPacingPolicy() noexcept
{
    MediaLatencyPolicy policy;
    policy.mode = MediaLatencyMode::Realtime;
    policy.enablePacing = true;
    return policy;
}

MediaRealtimeAvStartBarrierPlan planAvStartBarrierPolicy(bool includeAudio) noexcept
{
    MediaRealtimeAvStartBarrierPlan plan;
    plan.expectVideo = true;
    plan.expectAudio = includeAudio;
    plan.requireVideoKeyFrame = includeAudio;
    return plan;
}

} // namespace

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    if (auto status = validateExplicitStreamClassification(options); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    if (auto status = validateSupportedStreamClassification(options); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    if ((isRealtimeUrlInput(options) || isMpegTsUdpInput(options)) && options.input.url.empty()) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP input URL must be explicit"));
    }
    if (isSeparateRtpOutput(options)) {
        if (!options.output.packetSize.has_value() || *options.output.packetSize <= 0) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP output packet size must be explicit and positive"));
        }
        if (options.output.sdpPath.empty()) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument("Realtime RTP SDP output path must be explicit"));
        }
    }
    if (auto queueStatus = validateQueues(options.parameters.queues); !queueStatus) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(queueStatus.error());
    }

    if (isRealtimeUrlInput(options)) {
        if (auto status = validateRealtimeUrlInput(options); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
        }
    }
    if (isMpegTsUdpInput(options)) {
        if (auto status = validateMpegTsUdpInput(options); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
        }
    }

    if (auto status = validateRealtimeInputOpenOptions(options); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }

    auto outputUrls = isMuxedTransportStreamOutput(options)
        ? planMuxedTransportStreamOutput(options.output)
        : planOutputUrls(options.output, audioRequested(options));
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

    std::string videoRawRtpSdpText;
    std::string audioRawRtpSdpText;
    std::string plannedAudioInputUrl;
    std::string plannedInputUrl = options.input.url;
    MediaPipelinePlan videoPlan;
    MediaAudioPipelinePlan audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    if (isRtpPortInput(options)) {
        auto videoDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Video, options.input.videoRtp);
        if (!videoDescriptor) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(videoDescriptor.error());
        }
        auto videoEndpoint = validateRawRtpEndpoint(options.input.videoRtp, "Raw RTP video");
        if (!videoEndpoint) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(videoEndpoint.error());
        }

        if (audioRequested(options)) {
            auto audioDescriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Audio, options.input.audioRtp);
            if (!audioDescriptor) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioDescriptor.error());
            }
            auto audioEndpoint = validateRawRtpEndpoint(options.input.audioRtp, "Raw RTP audio");
            if (!audioEndpoint) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioEndpoint.error());
            }
            plannedAudioInputUrl = options.input.audioRtp.url;
            audioRawRtpSdpText = planSingleRawRtpSdp(audioEndpoint.value(),
                                                     options.input.audioRtp,
                                                     audioDescriptor.value(),
                                                     options.mediaId);

            MediaInputAudioStreamInfo audioInfo;
            audioInfo.streamIndex = RawRtpAudioStreamIndex;
            audioInfo.codecName = audioDescriptor.value().codecName;
            audioInfo.sampleRate = audioDescriptor.value().clockRate;
            audioInfo.channels = audioDescriptor.value().channels;
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

        videoRawRtpSdpText = planSingleRawRtpSdp(videoEndpoint.value(),
                                                 options.input.videoRtp,
                                                 videoDescriptor.value(),
                                                 options.mediaId);
        plannedInputUrl = options.input.videoRtp.url;

        MediaInputVideoStreamInfo inputInfo;
        inputInfo.streamIndex = RawRtpVideoStreamIndex;
        inputInfo.codecName = videoDescriptor.value().codecName;
        auto plannedVideoParameters = resolveRealtimeVideoParameters(options.parameters.video, inputInfo);
        if (!plannedVideoParameters) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideoParameters.error());
        }
        videoParameters = std::move(plannedVideoParameters).value();
        auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
            std::move(inputInfo),
            plannedInputUrl,
            std::move(pipelineOptions));
        if (!plannedVideo) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideo.error());
        }
        videoPlan = std::move(plannedVideo).value();
        videoPlan.synthesizeMissingTimestamps = true;
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
        auto plannedVideoParameters = resolveRealtimeVideoParameters(options.parameters.video, realtimeInput.value().video);
        if (!plannedVideoParameters) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideoParameters.error());
        }
        videoParameters = std::move(plannedVideoParameters).value();

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
    plan.inputType = *options.input.type;
    plan.inputLayout = *options.input.streamLayout;
    plan.outputLayout = *options.output.streamLayout;
    plan.videoPlan = std::move(videoPlan);
    plan.audioPlan = std::move(audioPlan);
    plan.videoParameters = std::move(videoParameters);
    plan.audioParameters = options.parameters.audio;
    plan.queues = options.parameters.queues;
    plan.edgePolicies = planEdgePolicies(options.parameters.queues);
    plan.threadingPolicy = planThreadingPolicy();
    plan.videoInputStartRequiresKeyFrame = isRtpPortInput(options);
    plan.input.url = plannedInputUrl;
    plan.input.sdpText = std::move(videoRawRtpSdpText);
    plan.input.rtspTransport = options.input.rtspTransport;
    plan.input.openTimeoutMs = *options.input.openTimeoutMs;
    plan.input.readTimeoutMs = *options.input.readTimeoutMs;
    plan.input.analyzeDurationUs = *options.input.analyzeDurationUs;
    plan.input.probeSizeBytes = *options.input.probeSizeBytes;
    plan.input.lowLatency = *options.input.lowLatency;
    plan.input.mediaId = options.mediaId;
    if (isRtpPortInput(options) && audioRequested(options)) {
        plan.useIsolatedAudioInput = true;
        plan.audioInput.url = std::move(plannedAudioInputUrl);
        plan.audioInput.sdpText = std::move(audioRawRtpSdpText);
        plan.audioInput.rtspTransport = options.input.rtspTransport;
        plan.audioInput.openTimeoutMs = *options.input.openTimeoutMs;
        plan.audioInput.readTimeoutMs = *options.input.readTimeoutMs;
        plan.audioInput.analyzeDurationUs = *options.input.analyzeDurationUs;
        plan.audioInput.probeSizeBytes = *options.input.probeSizeBytes;
        plan.audioInput.lowLatency = *options.input.lowLatency;
        plan.audioInput.mediaId = options.mediaId;
    }
    if (isSeparateRtpOutput(options)) {
        applyRealtimeOutputVideoRequirements(plan.videoParameters,
                                             plan.outputLayout,
                                             plan.videoPlan.outputCodecName);
        plan.videoOutput.url = outputUrls.value().videoUrl;
        plan.videoOutput.packetSize = *options.output.packetSize;
        plan.videoOutput.mediaId = options.mediaId;
        applyRtpOutputWritePacing(plan.videoOutput,
                                  static_cast<int64_t>(*plan.videoParameters.bitrateKbps) * 1000);
        plan.audioOutput.url = outputUrls.value().audioUrl;
        plan.audioOutput.packetSize = *options.output.packetSize;
        plan.audioOutput.mediaId = options.mediaId;
        applyRtpOutputWritePacing(plan.audioOutput,
                                  options.parameters.audio.bitrateKbps
                                      ? static_cast<int64_t>(*options.parameters.audio.bitrateKbps) * 1000
                                      : RealtimeDefaultAudioOutputBitsPerSecond);
        plan.sdp.path = options.output.sdpPath;
        plan.sdp.mediaId = options.mediaId;
        plan.sdp.expectedContexts = audioRequested(options) ? 2 : 1;
        plan.videoMux.expectVideo = true;
        plan.videoMux.expectAudio = false;
        plan.videoMux.pacingPolicy = planRtpMuxPacingPolicy();
        plan.videoMux.monotonicPacketTimestamps = true;
        plan.videoMux.startupDelayMs = RealtimeRtpSessionStartupDelayMs;
        plan.audioMux.expectVideo = false;
        plan.audioMux.expectAudio = audioRequested(options);
        plan.audioMux.pacingPolicy = planRtpMuxPacingPolicy();
        plan.audioMux.monotonicPacketTimestamps = audioRequested(options);
        plan.audioMux.startupDelayMs = audioRequested(options) ? RealtimeRtpSessionStartupDelayMs : 0;
        plan.avStartBarrier = planAvStartBarrierPolicy(audioRequested(options));
    } else {
        plan.muxedOutput.url = outputUrls.value().muxedUrl;
        plan.muxedOutput.format = outputUrls.value().muxedFormat;
        plan.muxedOutput.mediaId = options.mediaId;
        plan.videoMux.expectVideo = true;
        plan.videoMux.expectAudio = audioRequested(options);
        plan.audioMux.expectVideo = false;
        plan.audioMux.expectAudio = false;
    }
    return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph

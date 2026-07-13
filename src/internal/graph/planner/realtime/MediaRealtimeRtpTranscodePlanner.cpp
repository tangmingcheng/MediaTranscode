#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr int RealtimeNoBidirectionalFrames = 0;
constexpr int RealtimeDefaultGopFrames = 30;
constexpr bool RealtimeRequiresRuntimeAvailability = true;
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
    policy.collectWorkerMetrics = true;
    return policy;
}

} // namespace

::media::Result<MediaRealtimeTsInputPlan> MediaRealtimeTsInputPlan::create(
    std::size_t packetSize,
    std::uint64_t probeWindowBytes,
    std::uint64_t maximumPacketPositionRegressionBytes,
    std::size_t evidenceTimelineCapacity,
    std::size_t selectedStreamCount)
{
    auto minimum = minimumEvidenceCapacity(
        packetSize, probeWindowBytes, maximumPacketPositionRegressionBytes);
    if (!minimum) {
        return ::media::Result<MediaRealtimeTsInputPlan>::failure(minimum.error());
    }
    if (evidenceTimelineCapacity < minimum.value()) {
        return ::media::Result<MediaRealtimeTsInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence capacity is below worst-case requirement"));
    }
    if (selectedStreamCount == 0 || selectedStreamCount > 2) {
        return ::media::Result<MediaRealtimeTsInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("invalid MPEG-TS selected stream count"));
    }
    MediaRealtimeTsInputPlan result;
    result.demuxFormat = "mpegts";
    result.packetSize = packetSize;
    result.avioBufferBytes = 65'535;
    result.maximumDatagramBytes = 65'535;
    result.evidenceTimelineCapacity = evidenceTimelineCapacity;
    result.maximumPacketPositionRegressionBytes = maximumPacketPositionRegressionBytes;
    result.pesProvenanceCapacity =
        static_cast<std::size_t>((probeWindowBytes + packetSize - 1) / packetSize) +
        selectedStreamCount;
    result.packetOriginPolicy = MediaTsPacketOriginPolicy::PerStreamPesCarry;
    return ::media::Result<MediaRealtimeTsInputPlan>::success(std::move(result));
}

::media::Result<std::size_t> MediaRealtimeTsInputPlan::minimumEvidenceCapacity(
    std::size_t packetSize,
    std::uint64_t probeWindowBytes,
    std::uint64_t maximumPacketPositionRegressionBytes)
{
    if (packetSize != 188 || probeWindowBytes == 0 ||
        maximumPacketPositionRegressionBytes == 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("invalid MPEG-TS input evidence policy"));
    }
    const auto packetCount = [packetSize](std::uint64_t bytes)
        -> std::optional<std::uint64_t> {
        const auto stride = static_cast<std::uint64_t>(packetSize);
        if (bytes > std::numeric_limits<std::uint64_t>::max() - (stride - 1)) {
            return std::nullopt;
        }
        return (bytes + stride - 1) / stride;
    };
    const auto probePackets = packetCount(probeWindowBytes);
    const auto rollbackPackets = packetCount(maximumPacketPositionRegressionBytes);
    if (!probePackets || !rollbackPackets ||
        *probePackets > std::numeric_limits<std::uint64_t>::max() - *rollbackPackets) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence capacity arithmetic overflow"));
    }
    const std::uint64_t retainedPackets = *probePackets + *rollbackPackets;
    constexpr std::uint64_t CheckpointsPerPacket = 2;
    constexpr std::uint64_t PredecessorCheckpoint = 1;
    if (retainedPackets >
        (std::numeric_limits<std::uint64_t>::max() - PredecessorCheckpoint) /
            CheckpointsPerPacket) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS evidence checkpoint multiplication overflow"));
    }
    const std::uint64_t required =
        retainedPackets * CheckpointsPerPacket + PredecessorCheckpoint;
    if (required > (std::numeric_limits<std::size_t>::max)()) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence capacity exceeds platform size"));
    }
    return ::media::Result<std::size_t>::success(static_cast<std::size_t>(required));
}

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.type || *options.input.type != RealtimeInputType::RtpPort) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "URL and MPEG-TS realtime input require preflight() to preserve the prepared session"));
    }
    return planWithInput(options, nullptr, nullptr);
}

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::planWithInput(
    const MediaRealtimeRtpTranscodeRequest& options,
    const MediaRealtimeInputStreamInfo* preparedInput,
    const MediaTsSelectedProgramPlan* selectedTsProgram)
{
    if (auto status = validateRealtimeRequestNoIo(options); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }

    auto outputUrls = MediaRealtimeOutputPolicyPlanner::planUrls(options);
    if (!outputUrls) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputUrls.error());
    }

    auto pipelineOptionsResult = planVideoPipelineOptions(options, outputUrls.value().video);
    if (!pipelineOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(pipelineOptionsResult.error());
    }
    MediaPipelinePlannerOptions pipelineOptions = std::move(pipelineOptionsResult).value();

    auto audioOptionsResult = planAudioPipelineOptions(options);
    if (!audioOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioOptionsResult.error());
    }
    MediaAudioPipelinePlannerOptions audioOptions = std::move(audioOptionsResult).value();

    std::optional<MediaRealtimeRawInputPlan> rawInput;
    MediaPipelinePlan videoPlan;
    MediaAudioPipelinePlan audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options)) {
        auto raw = MediaRealtimeInputPlanner::planRawRtp(options);
        if (!raw) return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(raw.error());
        rawInput.emplace(std::move(raw).value());

        if (MediaRealtimeRequestClassifier::audioRequested(options)) {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudioTranscode(*rawInput->audio, audioOptions);
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

        auto plannedVideoParameters = resolveRealtimeVideoParameters(options.parameters.video, rawInput->video);
        if (!plannedVideoParameters) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideoParameters.error());
        }
        videoParameters = std::move(plannedVideoParameters).value();
        auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
            rawInput->video,
            rawInput->videoUrl,
            std::move(pipelineOptions));
        if (!plannedVideo) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideo.error());
        }
        videoPlan = std::move(plannedVideo).value();
        videoPlan.synthesizeMissingTimestamps = true;
    } else {
        MediaRealtimeInputStreamInfo realtimeInput;
        if (preparedInput) {
            realtimeInput = *preparedInput;
        } else {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Realtime URL and MPEG-TS planning requires prepared input stream information"));
        }

        auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
            realtimeInput.video,
            options.input.url,
            std::move(pipelineOptions));
        if (!plannedVideo) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideo.error());
        }
        videoPlan = std::move(plannedVideo).value();
        auto plannedVideoParameters = resolveRealtimeVideoParameters(options.parameters.video, realtimeInput.video);
        if (!plannedVideoParameters) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideoParameters.error());
        }
        videoParameters = std::move(plannedVideoParameters).value();

        if (MediaRealtimeRequestClassifier::audioRequested(options)) {
            if (!realtimeInput.hasAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::invalidArgument("Realtime RTP audio was requested but input has no audio stream"));
            }
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudioTranscode(realtimeInput.audio, audioOptions);
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
    plan.audioPacketNormalizationRequired = *options.input.type != RealtimeInputType::RtpPort;
    plan.videoPacketCopyNormalizationRequired = *options.input.type != RealtimeInputType::RtpPort;
    plan.videoPlan = std::move(videoPlan);
    plan.audioPlan = std::move(audioPlan);
    plan.videoParameters = std::move(videoParameters);
    plan.audioParameters = options.parameters.audio;
    plan.queues = options.parameters.queues;
    plan.edgePolicies = planEdgePolicies(options.parameters.queues);
    plan.threadingPolicy = planThreadingPolicy();
    plan.videoInputStartRequiresKeyFrame = MediaRealtimeRequestClassifier::unreliablePacketBoundary(options);
    MediaRealtimeInputPlanner::applyNodePlans(options, rawInput ? &*rawInput : nullptr, plan);
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(options)) {
        constexpr std::uint64_t MaximumRegressionBytes = 1024 * 1024;
        constexpr std::uint64_t PacketSize = 188;
        const auto probeBytes = static_cast<std::uint64_t>(*options.input.probeSizeBytes);
        auto capacity = MediaRealtimeTsInputPlan::minimumEvidenceCapacity(
            PacketSize, probeBytes, MaximumRegressionBytes);
        if (!capacity) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(capacity.error());
        }
        auto ts = MediaRealtimeTsInputPlan::create(
            PacketSize, probeBytes, MaximumRegressionBytes,
            capacity.value(), MediaRealtimeRequestClassifier::audioRequested(options) ? 2 : 1);
        if (!ts) return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(ts.error());
        plan.input.mpegTs = std::move(ts).value();
    }
    if (auto outputStatus = MediaRealtimeOutputPolicyPlanner::apply(options, outputUrls.value(), plan);
        !outputStatus) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputStatus.error());
    }
    if (MediaRealtimeRequestClassifier::audioRequested(options)) {
        auto avSync = MediaAvSyncPlanner::plan(options, selectedTsProgram);
        if (!avSync) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(avSync.error());
        }
        plan.avSync = std::move(avSync).value();
    }
    if (plan.input.mpegTs && selectedTsProgram) {
        auto& ts = *plan.input.mpegTs;
        ts.programNumber = selectedTsProgram->programNumber;
        ts.programMapPid = selectedTsProgram->programMapPid;
        ts.videoPid = selectedTsProgram->videoPid;
        ts.audioPid = selectedTsProgram->audioPid;
        ts.pcrPid = selectedTsProgram->pcrPid;
        ts.pcrInterval27Mhz = 540'000;
        ts.maximumPcrJitter27Mhz = 135'000;
        ts.maximumPcrGap27Mhz = 2'700'000;
        ts.projectionCapacity = ts.evidenceTimelineCapacity;
        ts.timestampTimeBaseNumerator = 1;
        ts.timestampTimeBaseDenominator = 90'000;
        ts.initialSourceGeneration = 0;
        ts.initialRawTransportGeneration = 0;
    }
    if (auto status = MediaRealtimeTsInputPlanValidator::validate(plan.inputType, plan.input);
        !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(std::move(plan));
}

::media::Result<MediaRealtimeTranscodePreflight> MediaRealtimeRtpTranscodePlanner::preflight(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    return preflightImpl(request, nullptr);
}

::media::Result<MediaRealtimeTranscodePreflight> MediaRealtimeRtpTranscodePlanner::preflight(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimePreflightIo& io)
{
    return preflightImpl(request, &io);
}

::media::Result<MediaRealtimeTranscodePreflight> MediaRealtimeRtpTranscodePlanner::preflightImpl(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimePreflightIo* io)
{
    if (auto status = validateRealtimeRequestNoIo(request); !status) {
        return ::media::Result<MediaRealtimeTranscodePreflight>::failure(status.error());
    }
    if (request.input.type && *request.input.type == RealtimeInputType::RtpPort) {
        auto planned = plan(request);
        if (!planned) return ::media::Result<MediaRealtimeTranscodePreflight>::failure(planned.error());
        MediaRealtimeTranscodePreflight result;
        result.plan = std::move(planned).value();
        return ::media::Result<MediaRealtimeTranscodePreflight>::success(std::move(result));
    }

    auto outputUrls = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    if (!outputUrls) {
        return ::media::Result<MediaRealtimeTranscodePreflight>::failure(outputUrls.error());
    }
    auto pipelineOptions = planVideoPipelineOptions(request, outputUrls.value().video);
    if (!pipelineOptions) {
        return ::media::Result<MediaRealtimeTranscodePreflight>::failure(pipelineOptions.error());
    }
    auto scanned = MediaRealtimeInputPlanner::prepare(request, pipelineOptions.value(), io);
    if (!scanned) {
        return ::media::Result<MediaRealtimeTranscodePreflight>::failure(scanned.error());
    }
    MediaPreparedRealtimeInputScan scan = std::move(scanned).value();
    auto planned = planWithInput(
        request, &scan.streams,
        scan.selectedTsProgram ? &*scan.selectedTsProgram : nullptr);
    if (!planned) {
        return ::media::Result<MediaRealtimeTranscodePreflight>::failure(planned.error());
    }
    MediaRealtimeTranscodePreflight result;
    result.plan = std::move(planned).value();
    result.prepared.emplace(std::move(scan.prepared));
    return ::media::Result<MediaRealtimeTranscodePreflight>::success(std::move(result));
}


::media::Status MediaRealtimeRtpTranscodePlanner::validateRealtimeRequestNoIo(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    return MediaRealtimeRequestValidator::validate(request);
}

} // namespace media::ffmpeg::graph

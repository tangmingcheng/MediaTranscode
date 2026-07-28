#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/capability/MediaSelectedEncoderPacketLayoutResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status planScheduledRtpPacketization(
    MediaRealtimeRtpTranscodePlan& plan,
    const MediaAvSyncPlan& synchronization)
{
    if (!synchronization.rtp || !synchronization.rtp->videoOutput.clockRate ||
        !synchronization.rtp->videoOutput.payloadType ||
        !synchronization.rtp->audioOutput.clockRate ||
        !synchronization.rtp->audioOutput.payloadType ||
        !plan.audioPlan.resolvedOutput || plan.videoOutput.packetSize <= 0 ||
        plan.audioOutput.packetSize <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "scheduled RTP packetization requires complete selected protocol facts"));
    }
    auto videoPacketLayout =
        MediaSelectedEncoderPacketLayoutResolver::require(
            plan.videoPlan,
            MediaEncodedPacketLayoutKind::StartCodeDelimited,
            "scheduled RTP H264 packetization");
    if (!videoPacketLayout) {
        return ::media::Status::failure(videoPacketLayout.error());
    }
    auto video = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, plan.videoPlan.outputCodecName, 1,
        *synchronization.rtp->videoOutput.clockRate,
        *synchronization.rtp->videoOutput.payloadType,
        static_cast<std::size_t>(plan.videoOutput.packetSize));
    auto audio = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, plan.audioPlan.resolvedOutput->codecName(), 1,
        *synchronization.rtp->audioOutput.clockRate,
        *synchronization.rtp->audioOutput.payloadType,
        static_cast<std::size_t>(plan.audioOutput.packetSize),
        plan.audioPlan.resolvedOutput->codecFrameSamples());
    if (!video || !audio) {
        return ::media::Status::failure(video ? audio.error() : video.error());
    }
    plan.videoOutput.scheduledPacketization = std::move(video).value();
    plan.audioOutput.scheduledPacketization = std::move(audio).value();
    return ::media::Status::success();
}

constexpr int RealtimeNoBidirectionalFrames = 0;
constexpr int RealtimeDefaultGopFrames = 30;

std::string planPreferredHardware(const MediaTranscodeExecutionParameters& execution)
{
    return execution.disableHardware ? "software" : "auto";
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
                                               options.parameters.execution.disableHardware,
                                               *options.input.lowLatency);
    plannerOptions.outputPath = outputUrl;
    plannerOptions.outputCodecName = video.codecName;
    plannerOptions.targetWidth = video.width.value_or(0);
    plannerOptions.targetHeight = video.height.value_or(0);
    plannerOptions.probeWidth = plannerOptions.targetWidth;
    plannerOptions.probeHeight = plannerOptions.targetHeight;
    if (video.frameRate.complete() && video.frameRate.numerator &&
        video.frameRate.denominator) {
        plannerOptions.probeFrameRate = MediaRational{
            *video.frameRate.numerator, *video.frameRate.denominator};
    }
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

::media::Result<std::int64_t> planMpegTsMaximumPcrGap27Mhz(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.mpegTsClock.maximumPcrGap) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS maximum PCR gap was not validated"));
    }
    constexpr std::int64_t NanosecondsPerMicrosecond = 1'000;
    constexpr std::int64_t PcrTicksPerMicrosecond = 27;
    const std::int64_t nanoseconds =
        request.input.mpegTsClock.maximumPcrGap->nanoseconds();
    if (nanoseconds <= 0 || nanoseconds % NanosecondsPerMicrosecond != 0) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maximum PCR gap must be positive and exactly representable at 27 MHz"));
    }
    const std::int64_t microseconds = nanoseconds / NanosecondsPerMicrosecond;
    if (microseconds > std::numeric_limits<std::int64_t>::max() /
            PcrTicksPerMicrosecond) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maximum PCR gap exceeds the 27 MHz policy range"));
    }
    return ::media::Result<std::int64_t>::success(
        microseconds * PcrTicksPerMicrosecond);
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
    plannerOptions.outputRequirement.requireFrameTranscode =
        MediaRealtimeRequestClassifier::audioRequested(options);
    if (MediaRealtimeRequestClassifier::muxedTransportOutput(options)) {
        plannerOptions.outputRequirement.codecName = "aac";
        plannerOptions.outputRequirement.profile = MediaAudioProfile::knownAacLow();
        plannerOptions.outputRequirement.sampleRate = 48'000;
        plannerOptions.outputRequirement.channels = 2;
    }
    plannerOptions.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    return ::media::Result<MediaAudioPipelinePlannerOptions>::success(std::move(plannerOptions));
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
    const MediaRealtimeRtpTranscodeRequest& requestedOptions,
    const MediaRealtimeInputStreamInfo* preparedInput,
    const MediaTsSelectedProgramPlan* selectedTsProgram)
{
    if (auto status = validateRealtimeRequestNoIo(requestedOptions); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    MediaRealtimeRtpTranscodeRequest options = requestedOptions;
    options.parameters.queues = MediaRealtimeQueueCapacityPlanner::plan(
        requestedOptions.parameters.queues);

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

    std::optional<MediaAvSyncPlan> plannedRawRtpAvSync;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options) &&
        MediaRealtimeRequestClassifier::audioRequested(options)) {
        auto avSync = MediaAvSyncPlanner::plan(options);
        if (!avSync) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                avSync.error());
        }
        plannedRawRtpAvSync.emplace(std::move(avSync).value());
    }

    std::optional<MediaRealtimeRawInputPlan> rawInput;
    MediaPipelinePlan videoPlan;
    MediaAudioPipelinePlan audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options)) {
        auto raw = MediaRealtimeInputPlanner::planRawRtp(
            options,
            plannedRawRtpAvSync ? &*plannedRawRtpAvSync : nullptr);
        if (!raw) return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(raw.error());
        rawInput.emplace(std::move(raw).value());

        if (MediaRealtimeRequestClassifier::audioRequested(options)) {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudio(*rawInput->audio, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan = std::move(plannedAudio).value();
        } else {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudio({}, audioOptions);
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
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudio(realtimeInput.audio, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan = std::move(plannedAudio).value();
        } else {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudio({}, audioOptions);
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
    plan.queues = options.parameters.queues;
    plan.edgePolicies = MediaRealtimeEdgePolicyPlanner::plan(plan.queues);
    plan.threadingPolicy = planThreadingPolicy();
    plan.videoInputStartRequiresKeyFrame = MediaRealtimeRequestClassifier::unreliablePacketBoundary(options);
    MediaRealtimeInputPlanner::applyNodePlans(options, rawInput ? &*rawInput : nullptr, plan);
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(options)) {
        auto maximumPcrGap27Mhz = planMpegTsMaximumPcrGap27Mhz(options);
        if (!maximumPcrGap27Mhz) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                maximumPcrGap27Mhz.error());
        }
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
        if (selectedTsProgram) {
            auto& selected = *plan.input.mpegTs;
            selected.programNumber = selectedTsProgram->programNumber;
            selected.programMapPid = selectedTsProgram->programMapPid;
            selected.videoPid = selectedTsProgram->videoPid;
            selected.audioPid = selectedTsProgram->audioPid;
            selected.pcrPid = selectedTsProgram->pcrPid;
            selected.videoPacketDuration =
                selectedTsProgram->videoPacketDuration;
            selected.audioPacketDuration =
                selectedTsProgram->audioPacketDuration;
            selected.maximumPcrGap27Mhz = maximumPcrGap27Mhz.value();
            selected.projectionCapacity = selected.evidenceTimelineCapacity;
            selected.timestampTimeBaseNumerator = 1;
            selected.timestampTimeBaseDenominator = 90'000;
            selected.initialSourceGeneration = MediaFirstLockedSourceGeneration;
            selected.initialRawTransportGeneration = 0;
        }
    }
    if (auto outputStatus = MediaRealtimeOutputPolicyPlanner::apply(options, outputUrls.value(), plan);
        !outputStatus) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputStatus.error());
    }
    std::optional<MediaProjectMpegTsResolvedPipelineFacts> resolvedTsFacts;
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(options) &&
        MediaRealtimeRequestClassifier::muxedTransportOutput(options)) {
        if (!plan.audioPlan.resolvedOutput) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output requires resolved audio format facts"));
        }
        auto videoPacketLayout =
            MediaSelectedEncoderPacketLayoutResolver::resolve(plan.videoPlan);
        if (!videoPacketLayout) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                videoPacketLayout.error());
        }
        resolvedTsFacts.emplace(MediaProjectMpegTsResolvedPipelineFacts{
            plan.videoPlan.outputCodecName,
            std::move(videoPacketLayout).value(),
            *plan.audioPlan.resolvedOutput});
    }
    if (MediaRealtimeRequestClassifier::audioRequested(options)) {
        auto avSync = plannedRawRtpAvSync
            ? ::media::Result<MediaAvSyncPlan>::success(
                  std::move(*plannedRawRtpAvSync))
            : MediaAvSyncPlanner::plan(
                  options, selectedTsProgram,
                  resolvedTsFacts ? &*resolvedTsFacts : nullptr);
        if (!avSync) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(avSync.error());
        }
        if (plan.input.mpegTs) {
            const auto& startup = avSync.value().startup;
            if (!startup.videoCapacity || !startup.audioCapacity ||
                !startup.videoByteCapacity || !startup.audioByteCapacity ||
                !startup.maximumVideoUnitBytes ||
                !startup.maximumAudioUnitBytes) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS acquiring retention requires complete planner startup bounds"));
            }
            auto& ts = *plan.input.mpegTs;
            ts.initialAcquiringVideoPacketCapacity = *startup.videoCapacity;
            ts.initialAcquiringAudioPacketCapacity = *startup.audioCapacity;
            ts.initialAcquiringVideoByteCapacity = *startup.videoByteCapacity;
            ts.initialAcquiringAudioByteCapacity = *startup.audioByteCapacity;
            ts.maximumAcquiringVideoPacketBytes = *startup.maximumVideoUnitBytes;
            ts.maximumAcquiringAudioPacketBytes = *startup.maximumAudioUnitBytes;
        }
        auto componentBounds =
            MediaRealtimeAvSyncComponentBoundsPlanner::plan(plan);
        if (!componentBounds) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                componentBounds.error());
        }
        plan.avSyncComponentBounds = std::move(componentBounds).value();
        if (avSync.value().topology ==
            MediaAvSyncTopology::SeparateRtpToSeparateRtp) {
            if (auto status = planScheduledRtpPacketization(
                    plan, avSync.value()); !status) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    status.error());
            }
        }
        auto runtime = MediaRealtimeAvSyncRuntimePlanner::plan(
            plan, std::move(avSync).value());
        if (!runtime) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                runtime.error());
        }
        plan.avSyncRuntime = std::move(runtime).value();
        plan.audioPacketNormalizationRequired = false;
        plan.videoPacketCopyNormalizationRequired = false;
        plan.videoOutput.writePacingEnabled = false;
        plan.videoOutput.writePacingBytesPerSecond = 0;
        plan.videoOutput.writePacingBurstBytes = 0;
        plan.audioOutput.writePacingEnabled = false;
        plan.audioOutput.writePacingBytesPerSecond = 0;
        plan.audioOutput.writePacingBurstBytes = 0;
        plan.videoMux.pacingPolicy = {};
        plan.videoMux.startupDelayMs = 0;
        plan.audioMux.pacingPolicy = {};
        plan.audioMux.startupDelayMs = 0;
        plan.avStartBarrier = {};
    }
    if (auto status = MediaRealtimeTsInputPlanValidator::validate(plan.inputType, plan.input);
        !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    if (auto status = validatePlannedProduct(plan); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            status.error());
    }
    return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(std::move(plan));
}

::media::Result<MediaRealtimeRtpTranscodePlan>
MediaRealtimeRtpTranscodePlanner::planPreparedInput(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeInputStreamInfo& input,
    const MediaTsSelectedProgramPlan& selectedTsProgram)
{
    return planWithInput(request, &input, &selectedTsProgram);
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
    auto planned = scan.selectedTsProgram
        ? planPreparedInput(request, scan.streams, *scan.selectedTsProgram)
        : planWithInput(request, &scan.streams, nullptr);
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

::media::Status MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
    const MediaRealtimeRtpTranscodePlan& plan)
{
    return MediaRealtimeAvSyncRuntimePlanValidator::validate(plan);
}
} // namespace media::ffmpeg::graph

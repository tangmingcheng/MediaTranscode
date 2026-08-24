#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/planner/realtime/MediaRtpIngressCapabilityMaterializer.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/capability/MediaSelectedEncoderPacketLayoutResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAudioPlannerOptionsResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRuntimePlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpInputPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpVideoSignalingResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanner.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramContractValidator.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaScheduledRtpPacketizationPlan>
planVideoScheduledRtpPacketization(
    const MediaRealtimeRtpTranscodePlanCore& plan,
    const MediaRealtimeScheduledRtpOutputPlanningDraft& output,
    int clockRate,
    int payloadType)
{
    if (output.packetSize <= 0 || clockRate <= 0 || payloadType < 0 ||
        payloadType > 127) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled video RTP packetization requires complete protocol facts"));
    }
    auto videoPacketLayout =
        MediaSelectedEncoderPacketLayoutResolver::require(
            plan.videoPlan,
            MediaEncodedPacketLayoutKind::StartCodeDelimited,
            "scheduled RTP H264 packetization");
    if (!videoPacketLayout) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            videoPacketLayout.error());
    }
    return MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, plan.videoPlan.outputCodecName, 1,
        clockRate, payloadType,
        static_cast<std::size_t>(output.packetSize));
}

::media::Status planScheduledRtpPacketization(
    const MediaRealtimeRtpTranscodePlanCore& plan,
    const MediaAudioPipelinePlan& audio,
    MediaRealtimeOutputPlanningDraft& output,
    const MediaAvSyncPlan& synchronization)
{
    if (!synchronization.rtpOutput ||
        !synchronization.rtpOutput->videoOutput.clockRate ||
        !synchronization.rtpOutput->videoOutput.payloadType ||
        !synchronization.rtpOutput->audioOutput.clockRate ||
        !synchronization.rtpOutput->audioOutput.payloadType ||
        !audio.resolvedOutput || output.audioOutput.packetSize <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "scheduled RTP packetization requires complete selected protocol facts"));
    }
    auto video = planVideoScheduledRtpPacketization(
        plan, output.videoOutput,
        *synchronization.rtpOutput->videoOutput.clockRate,
        *synchronization.rtpOutput->videoOutput.payloadType);
    auto audioPacketization = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, audio.resolvedOutput->codecName(), 1,
        *synchronization.rtpOutput->audioOutput.clockRate,
        *synchronization.rtpOutput->audioOutput.payloadType,
        static_cast<std::size_t>(output.audioOutput.packetSize),
        audio.resolvedOutput->codecFrameSamples());
    if (!video || !audioPacketization) {
        return ::media::Status::failure(
            video ? audioPacketization.error() : video.error());
    }
    output.videoOutput.scheduledPacketization = std::move(video).value();
    output.audioOutput.scheduledPacketization =
        std::move(audioPacketization).value();
    return ::media::Status::success();
}

::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>
preparedDemuxTimestampFacts(
    const MediaRealtimeRtpTranscodePlanCore& plan,
    const MediaAudioPipelinePlan& audio,
    const MediaPreparedRealtimeInput* prepared)
{
    if (!prepared) {
        return ::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "URL A/V planning requires the prepared input snapshot"));
    }
    const auto* video =
        prepared->inputStreamSnapshot(plan.videoPlan.sourceStreamIndex);
    const auto* audioStream =
        prepared->inputStreamSnapshot(audio.sourceStreamIndex);
    const auto* genericPlan = prepared->genericPlan();
    const auto* genericEvidence = prepared->genericEvidence();
    const auto* genericStartup = prepared->genericStartup();
    if (!video || !audioStream || !genericPlan || !genericEvidence ||
        !genericStartup ||
        video->index < 0 ||
        audioStream->index < 0 || video->index == audioStream->index ||
        !video->time.timeBase.isKnown() ||
        video->time.timeBase.num <= 0 || video->time.timeBase.den <= 0 ||
        !audioStream->time.timeBase.isKnown() ||
        audioStream->time.timeBase.num <= 0 ||
        audioStream->time.timeBase.den <= 0) {
        return ::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "URL A/V planning requires explicit prepared stream time bases"));
    }
    return ::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>::success(
        MediaAvSyncPreparedDemuxTimestampFacts{
            video->index, video->time.timeBase,
            audioStream->index, audioStream->time.timeBase,
            *genericPlan, *genericEvidence, *genericStartup});
}

constexpr int RealtimeNoBidirectionalFrames = 0;

MediaVideoTranscodeParameters planRealtimeVideoParameters(const MediaVideoTranscodeParameters& requested)
{
    MediaVideoTranscodeParameters planned = requested;
    planned.bFrames = RealtimeNoBidirectionalFrames;
    return planned;
}

::media::Result<MediaVideoTranscodeParameters> resolveRealtimeVideoParameters(
    const MediaVideoTranscodeParameters& requested,
    const MediaInputVideoStreamInfo& inputInfo)
{
    MediaVideoTranscodeParameters planned = planRealtimeVideoParameters(requested);
    if (!planned.gop || *planned.gop <= 0) {
        return ::media::Result<MediaVideoTranscodeParameters>::failure(
            ::media::ErrorInfo::notInitialized(
                "Realtime output GOP must be an explicit positive encoding request"));
    }
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
    if (!planned.frameRate.specified()) {
        if (!inputInfo.frameRate.isKnown()) {
            return ::media::Result<MediaVideoTranscodeParameters>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Realtime output frame rate cannot inherit an unobserved input frame rate"));
        }
        planned.frameRate.numerator = inputInfo.frameRate.num;
        planned.frameRate.denominator = inputInfo.frameRate.den;
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
    plannerOptions.encoderRateControl = MediaEncoderRateControlRequest{
        video.rateControl, video.bitrateKbps, video.minBitrateKbps,
        video.maxBitrateKbps, video.bufferSizeKbits};
    if (video.frameRate.complete() && video.frameRate.numerator &&
        video.frameRate.denominator) {
        plannerOptions.targetFrameRate = MediaRational{
            *video.frameRate.numerator, *video.frameRate.denominator};
    }
    plannerOptions.hardwareBackend = options.parameters.execution.hardwareBackend;
    plannerOptions.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    plannerOptions.rtspTransport = options.input.rtspTransport;
    plannerOptions.openTimeoutMs = *options.input.openTimeoutMs;
    plannerOptions.readTimeoutMs = *options.input.readTimeoutMs;
    plannerOptions.analyzeDurationUs = *options.input.analyzeDurationUs;
    plannerOptions.probeSizeBytes = *options.input.probeSizeBytes;
    return ::media::Result<MediaPipelinePlannerOptions>::success(std::move(plannerOptions));
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

MediaThreadingPolicy planThreadingPolicy() noexcept
{
    MediaThreadingPolicy policy;
    policy.mode = MediaThreadingMode::PerNodeWorker;
    policy.priority = MediaThreadPriority::High;
    policy.collectWorkerMetrics = true;
    return policy;
}

::media::Result<MediaPipelinePlan> planRawRtpVideoPipeline(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaDetectedRtpVideoSignaling& detected,
    const MediaRational& detectedFrameRate)
{
    auto signaling = MediaRealtimeRtpVideoSignalingResolver::resolve(
        request.input.videoRtp, &detected);
    if (!signaling) {
        return ::media::Result<MediaPipelinePlan>::failure(
            signaling.error());
    }
    auto outputUrls = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    if (!outputUrls) {
        return ::media::Result<MediaPipelinePlan>::failure(outputUrls.error());
    }
    auto pipelineOptions = planVideoPipelineOptions(
        request, outputUrls.value().video);
    if (!pipelineOptions) {
        return ::media::Result<MediaPipelinePlan>::failure(
            pipelineOptions.error());
    }
    pipelineOptions.value().probeWidth = signaling.value().codedSize.width;
    pipelineOptions.value().probeHeight = signaling.value().codedSize.height;
    pipelineOptions.value().sourceFrameRate = detectedFrameRate;
    MediaInputVideoStreamInfo input;
    input.streamIndex = MediaRealtimeRawInputPlan::VideoStreamIndex;
    input.codecName = canonicalCodecName(request.input.videoRtp.codecName);
    input.width = signaling.value().codedSize.width;
    input.height = signaling.value().codedSize.height;
    input.frameRate = detectedFrameRate;
    return MediaPipelinePlanner::planVideoTranscodeKnownInput(
        std::move(input), request.input.videoRtp.url,
        std::move(pipelineOptions).value());
}

::media::Status validatePreplannedRawRtpVideo(
    const MediaPipelinePlan& plan,
    const MediaInputVideoStreamInfo& input,
    const std::string& inputUrl,
    const std::string& outputUrl)
{
    if (!plan.enabled ||
        plan.sourceStreamIndex != input.streamIndex ||
        plan.inputCodecName != canonicalCodecName(input.codecName) ||
        plan.inputPath != inputUrl || plan.outputPath != outputUrl) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "preplanned raw RTP video pipeline conflicts with resolved input"));
    }
    return ::media::Status::success();
}

::media::Result<int> remainingRawRtpStartupMilliseconds(
    std::chrono::steady_clock::time_point deadline,
    const char* phase)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return ::media::Result<int>::failure(::media::ErrorInfo::wouldBlock(
            std::string("raw RTP preflight reached total open timeout during ") +
            phase));
    }
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
        deadline - now);
    if (remaining.count() <= 0 ||
        remaining.count() > (std::numeric_limits<int>::max)()) {
        return ::media::Result<int>::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP preflight deadline is outside the supported range"));
    }
    return ::media::Result<int>::success(
        static_cast<int>(remaining.count()));
}

} // namespace

MediaRealtimeTsInputPolicy::MediaRealtimeTsInputPolicy(
    std::string selectedDemuxFormat,
    std::size_t selectedPacketSize,
    std::size_t selectedAvioBufferBytes,
    std::size_t selectedMaximumDatagramBytes,
    std::size_t selectedEvidenceTimelineCapacity,
    std::uint64_t selectedMaximumPacketPositionRegressionBytes,
    std::size_t selectedPesProvenanceCapacity,
    MediaTsPacketOriginPolicy selectedPacketOriginPolicy) noexcept
    : demuxFormat(std::move(selectedDemuxFormat)),
      packetSize(selectedPacketSize),
      avioBufferBytes(selectedAvioBufferBytes),
      maximumDatagramBytes(selectedMaximumDatagramBytes),
      evidenceTimelineCapacity(selectedEvidenceTimelineCapacity),
      maximumPacketPositionRegressionBytes(
          selectedMaximumPacketPositionRegressionBytes),
      pesProvenanceCapacity(selectedPesProvenanceCapacity),
      packetOriginPolicy(selectedPacketOriginPolicy)
{
}

::media::Result<MediaRealtimeTsInputPlan::Retention> planMpegTsRetention(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    auto capacity = MediaRealtimeMediaCapacityPlanner::plan(request);
    if (!capacity) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            capacity.error());
    }
    const auto packetCapacity = capacity.value().videoUnits;
    const auto videoUnitBytes = capacity.value().videoUnitBytes;
    const auto videoByteCapacity = capacity.value().videoBytes;
    const auto maximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (videoByteCapacity > maximumSerialized) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video retention exceeds runtime option range"));
    }
    if (request.parameters.execution.streamSet ==
        MediaTranscodeStreamSet::VideoOnly) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::success(
            MediaRealtimeTsInputPlan::VideoOnlyRetention(
                packetCapacity, videoByteCapacity, videoUnitBytes));
    }
    if (!capacity.value().audioUnits || !capacity.value().audioUnitBytes ||
        !capacity.value().audioBytes) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::notInitialized(
                "AudioVideo MPEG-TS retention requires planned audio bounds"));
    }
    const auto audioUnitBytes = *capacity.value().audioUnitBytes;
    const auto audioByteCapacity = *capacity.value().audioBytes;
    if (audioByteCapacity > maximumSerialized) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS retention exceeds runtime option range"));
    }
    return ::media::Result<MediaRealtimeTsInputPlan::Retention>::success(
        MediaRealtimeTsInputPlan::AudioVideoRetention(
            packetCapacity, *capacity.value().audioUnits,
            videoByteCapacity, audioByteCapacity,
            videoUnitBytes, audioUnitBytes));
}

::media::Result<MediaRealtimeTsInputPolicy> MediaRealtimeTsInputPolicy::create(
    std::size_t packetSize,
    std::uint64_t probeWindowBytes,
    std::uint64_t maximumPacketPositionRegressionBytes,
    std::size_t evidenceTimelineCapacity,
    std::size_t selectedStreamCount)
{
    auto minimum = minimumEvidenceCapacity(
        packetSize, probeWindowBytes, maximumPacketPositionRegressionBytes);
    if (!minimum) {
        return ::media::Result<MediaRealtimeTsInputPolicy>::failure(minimum.error());
    }
    if (evidenceTimelineCapacity < minimum.value()) {
        return ::media::Result<MediaRealtimeTsInputPolicy>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS evidence capacity is below worst-case requirement"));
    }
    if (selectedStreamCount == 0 || selectedStreamCount > 2) {
        return ::media::Result<MediaRealtimeTsInputPolicy>::failure(
            ::media::ErrorInfo::invalidArgument("invalid MPEG-TS selected stream count"));
    }
    return ::media::Result<MediaRealtimeTsInputPolicy>::success(
        MediaRealtimeTsInputPolicy(
            "mpegts", packetSize, 65'535, 65'535,
            evidenceTimelineCapacity,
            maximumPacketPositionRegressionBytes,
            static_cast<std::size_t>(
                (probeWindowBytes + packetSize - 1) / packetSize) +
                selectedStreamCount,
            MediaTsPacketOriginPolicy::PerStreamPesCarry));
}

::media::Result<std::size_t> MediaRealtimeTsInputPolicy::minimumEvidenceCapacity(
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

MediaRealtimeTsInputPlan::MediaRealtimeTsInputPlan(
    MediaRealtimeTsInputPolicy policy,
    MediaTsSelectedProgramPlan selected,
    std::int64_t selectedMaximumPcrGap27Mhz,
    Retention selectedRetention,
    std::uint64_t selectedInitialSourceGeneration,
    std::uint64_t selectedInitialRawTransportGeneration) noexcept
    : demuxFormat(std::move(policy.demuxFormat)),
      packetSize(policy.packetSize),
      avioBufferBytes(policy.avioBufferBytes),
      maximumDatagramBytes(policy.maximumDatagramBytes),
      evidenceTimelineCapacity(policy.evidenceTimelineCapacity),
      maximumPacketPositionRegressionBytes(
          policy.maximumPacketPositionRegressionBytes),
      pesProvenanceCapacity(policy.pesProvenanceCapacity),
      packetOriginPolicy(policy.packetOriginPolicy),
      selectedProgram(std::move(selected)),
      maximumPcrGap27Mhz(selectedMaximumPcrGap27Mhz),
      projectionCapacity(evidenceTimelineCapacity),
      retention(std::move(selectedRetention)),
      initialSourceGeneration(selectedInitialSourceGeneration),
      initialRawTransportGeneration(selectedInitialRawTransportGeneration)
{
}

::media::Result<MediaRealtimeTsInputPlan> MediaRealtimeTsInputPlan::create(
    MediaRealtimeTsInputPolicy policy,
    MediaTsSelectedProgramPlan selectedProgram,
    std::int64_t maximumPcrGap27Mhz,
    Retention retention,
    std::uint64_t initialSourceGeneration,
    std::uint64_t initialRawTransportGeneration)
{
    MediaRealtimeTsInputPlan plan(
        std::move(policy), std::move(selectedProgram), maximumPcrGap27Mhz,
        std::move(retention), initialSourceGeneration,
        initialRawTransportGeneration);
    if (auto status = plan.validateProduct(); !status) {
        return ::media::Result<MediaRealtimeTsInputPlan>::failure(
            status.error());
    }
    return ::media::Result<MediaRealtimeTsInputPlan>::success(std::move(plan));
}

::media::Status MediaRealtimeTsInputPlan::validateProduct() const
{
    const bool audioVideoProgram = std::holds_alternative<
        MediaTsAudioVideoSelectedProgramPlan>(selectedProgram);
    const bool audioVideoRetention = std::holds_alternative<
        AudioVideoRetention>(retention);
    const bool retentionValid = audioVideoProgram == audioVideoRetention &&
        std::visit(
            [](const auto& selected) {
                using SelectedRetention = std::decay_t<decltype(selected)>;
                const bool videoValid = selected.videoPacketCapacity > 0 &&
                    selected.videoByteCapacity > 0 &&
                    selected.maximumVideoPacketBytes > 0 &&
                    selected.maximumVideoPacketBytes <=
                        selected.videoByteCapacity;
                if constexpr (std::is_same_v<
                                  SelectedRetention,
                                  AudioVideoRetention>) {
                    return videoValid &&
                        selected.audioPacketCapacity > 0 &&
                        selected.audioByteCapacity > 0 &&
                        selected.maximumAudioPacketBytes > 0 &&
                        selected.maximumAudioPacketBytes <=
                            selected.audioByteCapacity;
                }
                return videoValid;
            },
            retention);
    if (demuxFormat != "mpegts" || packetSize != 188 ||
        avioBufferBytes == 0 || maximumDatagramBytes == 0 ||
        maximumDatagramBytes > avioBufferBytes ||
        evidenceTimelineCapacity == 0 ||
        maximumPacketPositionRegressionBytes == 0 ||
        pesProvenanceCapacity == 0 ||
        packetOriginPolicy != MediaTsPacketOriginPolicy::PerStreamPesCarry ||
        !MediaTsProgramContractValidator::validateSelectedProgram(
            selectedProgram) ||
        maximumPcrGap27Mhz <= 0 || projectionCapacity == 0 ||
        !retentionValid) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "invalid complete MPEG-TS input plan"));
    }
    return ::media::Status::success();
}

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.type || *options.input.type != RealtimeInputType::RtpPort) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "URL and MPEG-TS realtime input require preflight() to preserve the prepared input contract"));
    }
    return planWithInput(
        options, nullptr, nullptr, nullptr, nullptr, nullptr, std::nullopt, nullptr,
        nullptr);
}

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::planWithInput(
    const MediaRealtimeRtpTranscodeRequest& requestedOptions,
    const MediaRealtimeInputStreamInfo* preparedInput,
    const MediaTsSelectedProgramPlan* selectedTsProgram,
    const MediaPreparedRealtimeInput* preparedResource,
    const MediaPreparedRealtimeInput* preparedAudioResource,
    const MediaRtpIngressPlan* preparedVideoIngress,
    std::optional<MediaPipelinePlan> preplannedVideo,
    const MediaDetectedRtpVideoSignaling* detectedVideoSignaling,
    const MediaRational* detectedVideoFrameRate)
{
    if (auto status = validateRealtimeRequestNoIo(requestedOptions); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    MediaRealtimeRtpTranscodeRequest options = requestedOptions;
    std::optional<MediaSize> rawRtpCodedSize;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options)) {
        auto resolvedSignaling =
            MediaRealtimeRtpVideoSignalingResolver::resolve(
                options.input.videoRtp, detectedVideoSignaling);
        if (!resolvedSignaling) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                resolvedSignaling.error());
        }
        options.input.videoRtp.fmtp = resolvedSignaling.value().fmtp;
        rawRtpCodedSize = resolvedSignaling.value().codedSize;
    } else if (detectedVideoSignaling) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "detected RTP video signaling is valid only for raw RTP input"));
    }
    auto selectedQueues = MediaRealtimeQueueCapacityPlanner::plan(
        *requestedOptions.deployment);
    if (!selectedQueues) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            selectedQueues.error());
    }
    options.parameters.queues = std::move(selectedQueues).value();

    auto outputUrls = MediaRealtimeOutputPolicyPlanner::planUrls(options);
    if (!outputUrls) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputUrls.error());
    }

    auto pipelineOptionsResult = planVideoPipelineOptions(options, outputUrls.value().video);
    if (!pipelineOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(pipelineOptionsResult.error());
    }
    MediaPipelinePlannerOptions pipelineOptions = std::move(pipelineOptionsResult).value();
    if (rawRtpCodedSize) {
        pipelineOptions.probeWidth = rawRtpCodedSize->width;
        pipelineOptions.probeHeight = rawRtpCodedSize->height;
    }
    if (detectedVideoFrameRate) {
        if (!detectedVideoFrameRate->isKnown()) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "detected raw RTP video frame rate is invalid"));
        }
        pipelineOptions.sourceFrameRate = *detectedVideoFrameRate;
    }

    auto audioOptionsResult =
        MediaRealtimeAudioPlannerOptionsResolver::resolve(options);
    if (!audioOptionsResult) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(audioOptionsResult.error());
    }
    MediaAudioPipelinePlannerOptions audioOptions = std::move(audioOptionsResult).value();

    std::optional<MediaAvSyncPlan> plannedRawRtpAvSync;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options) &&
        options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        auto rtpInput = MediaAvSyncPlanner::planRtpInputClock(options);
        if (!rtpInput) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                rtpInput.error());
        }
        plannedRawRtpAvSync.emplace();
        plannedRawRtpAvSync->sourceClockMode =
            MediaAvSyncSourceClockMode::RtpSenderReports;
        plannedRawRtpAvSync->rtpInput = std::move(rtpInput).value();
    }

    std::optional<MediaRealtimeRawInputPlan> rawInput;
    MediaPipelinePlan videoPlan;
    std::optional<MediaAudioPipelinePlan> audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options)) {
        auto raw = MediaRealtimeInputPlanner::planRawRtp(
            options,
            plannedRawRtpAvSync ? &*plannedRawRtpAvSync : nullptr);
        if (!raw) return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(raw.error());
        rawInput.emplace(std::move(raw).value());
        rawInput->video.width = pipelineOptions.probeWidth;
        rawInput->video.height = pipelineOptions.probeHeight;
        rawInput->video.frameRate = pipelineOptions.sourceFrameRate;

        if (options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudio(*rawInput->audio, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan.emplace(std::move(plannedAudio).value());
        }

        auto plannedVideoParameters = resolveRealtimeVideoParameters(options.parameters.video, rawInput->video);
        if (!plannedVideoParameters) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedVideoParameters.error());
        }
        videoParameters = std::move(plannedVideoParameters).value();
        if (preplannedVideo) {
            if (auto status = validatePreplannedRawRtpVideo(
                    *preplannedVideo, rawInput->video, rawInput->videoUrl,
                    outputUrls.value().video);
                !status) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    status.error());
            }
            videoPlan = std::move(*preplannedVideo);
        } else {
            auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeKnownInput(
                rawInput->video,
                rawInput->videoUrl,
                std::move(pipelineOptions));
            if (!plannedVideo) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    plannedVideo.error());
            }
            videoPlan = std::move(plannedVideo).value();
        }
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

        if (options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
            if (!realtimeInput.hasAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::invalidArgument("Realtime RTP audio was requested but input has no audio stream"));
            }
            auto plannedAudio = MediaAudioPipelinePlanner::planKnownAudio(realtimeInput.audio, audioOptions);
            if (!plannedAudio) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(plannedAudio.error());
            }
            audioPlan.emplace(std::move(plannedAudio).value());
        }
    }

    MediaRealtimeRtpTranscodePlanningDraft plan;
    plan.inputType = *options.input.type;
    plan.inputLayout = *options.input.streamLayout;
    plan.outputLayout = *options.output.streamLayout;
    plan.outputTransport = *options.output.transport;
    plan.videoPlan = std::move(videoPlan);
    plan.audioPlan = std::move(audioPlan);
    plan.videoParameters = std::move(videoParameters);
    plan.queues = options.parameters.queues;
    plan.edgePolicies = MediaRealtimeEdgePolicyPlanner::plan(plan.queues);
    plan.threadingPolicy = planThreadingPolicy();
    if (MediaRealtimeRequestClassifier::realtimeUrlInput(options)) {
        plan.requiredPreparedInputKind = MediaPreparedRealtimeInputKind::Generic;
    } else if (MediaRealtimeRequestClassifier::mpegTsUdpInput(options)) {
        plan.requiredPreparedInputKind = MediaPreparedRealtimeInputKind::MpegTs;
    } else if (preparedResource) {
        const auto preparedKind = preparedResource->kind();
        if (!preparedKind || *preparedKind != MediaPreparedRealtimeInputKind::RawRtp) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP automatic signaling requires prepared raw RTP input"));
        }
        plan.requiredPreparedInputKind = *preparedKind;
    }
    if (preparedAudioResource &&
        (!preparedAudioResource->kind() ||
         *preparedAudioResource->kind() !=
             MediaPreparedRealtimeInputKind::RawRtp)) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP automatic signaling requires prepared raw RTP audio input"));
    }
    plan.videoInputStartRequiresKeyFrame = MediaRealtimeRequestClassifier::unreliablePacketBoundary(options);
    MediaRealtimeInputPlanner::applyNodePlans(options, rawInput ? &*rawInput : nullptr, plan);
    if (MediaRealtimeRequestClassifier::rawRtpInput(options)) {
        plan.input.requiresPreparedInput =
            plan.requiredPreparedInputKind == MediaPreparedRealtimeInputKind::RawRtp;
        if (preparedVideoIngress) {
            if (!plan.input.rtpTransport || !preparedResource) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "planner-bound RTP ingress requires its prepared video transport"));
            }
            if (auto status = preparedVideoIngress->validateProduct(); !status) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    status.error());
            }
            plan.input.rtpTransport->ingress = *preparedVideoIngress;
            if (preparedVideoIngress->socketReceiveCapacityBytes() >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
                preparedVideoIngress->maximumDatagramBytes() >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
                preparedVideoIngress->reorderWindowPackets() >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "planner-bound RTP ingress facts exceed node option range"));
            }
            plan.input.rtpTransport->receiveBufferBytes = static_cast<int>(
                preparedVideoIngress->socketReceiveCapacityBytes());
            plan.input.rtpTransport->maximumDatagramBytes = static_cast<int>(
                preparedVideoIngress->maximumDatagramBytes());
            plan.input.rtpTransport->reorderWindowPackets =
                preparedVideoIngress->reorderWindowPackets();
            const auto delayNanoseconds =
                preparedVideoIngress->maximumReorderDelayNanoseconds();
            plan.input.rtpTransport->maximumReorderDelayMs =
                static_cast<int>((delayNanoseconds + 999'999) / 1'000'000);
        }
        if (plan.isolatedAudioInput) {
            if (*plan.input.requiresPreparedInput &&
                !preparedAudioResource) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "raw RTP automatic signaling requires synchronized prepared audio input"));
            }
            plan.isolatedAudioInput->requiresPreparedInput =
                *plan.input.requiresPreparedInput;
        } else if (preparedAudioResource) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP video-only plan rejects prepared audio input"));
        }
    }
    if (MediaRealtimeRequestClassifier::mpegTsUdpInput(options)) {
        if (!selectedTsProgram) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS planning requires a selected program"));
        }
        auto maximumPcrGap27Mhz = planMpegTsMaximumPcrGap27Mhz(options);
        if (!maximumPcrGap27Mhz) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                maximumPcrGap27Mhz.error());
        }
        constexpr std::uint64_t MaximumRegressionBytes = 1024 * 1024;
        constexpr std::uint64_t PacketSize = 188;
        const auto probeBytes = static_cast<std::uint64_t>(*options.input.probeSizeBytes);
        auto capacity = MediaRealtimeTsInputPolicy::minimumEvidenceCapacity(
            PacketSize, probeBytes, MaximumRegressionBytes);
        if (!capacity) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(capacity.error());
        }
        auto policy = MediaRealtimeTsInputPolicy::create(
            PacketSize, probeBytes, MaximumRegressionBytes,
            capacity.value(),
            options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo
                ? 2
                : 1);
        if (!policy) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                policy.error());
        }
        const bool expectsAudioVideo =
            options.parameters.execution.streamSet ==
            MediaTranscodeStreamSet::AudioVideo;
        if (expectsAudioVideo != std::holds_alternative<
                MediaTsAudioVideoSelectedProgramPlan>(*selectedTsProgram)) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS selected program stream set conflicts with request"));
        }
        auto retention = planMpegTsRetention(options);
        if (!retention) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                retention.error());
        }
        auto ts = MediaRealtimeTsInputPlan::create(
            std::move(policy).value(), *selectedTsProgram,
            maximumPcrGap27Mhz.value(), std::move(retention).value(),
            MediaFirstLockedSourceGeneration, 0);
        if (!ts) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ts.error());
        }
        plan.input.mpegTs = std::move(ts).value();
    }
    MediaRealtimeOutputPlanningDraft output;
    output.packetCopyNormalizationRequired =
        *options.input.type != RealtimeInputType::RtpPort;
    if (auto outputStatus = MediaRealtimeOutputPolicyPlanner::apply(
            options, outputUrls.value(), plan, output);
        !outputStatus) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(outputStatus.error());
    }
    MediaRational outputFrameRate;
    if (rawInput) {
        outputFrameRate = rawInput->video.frameRate;
    } else if (preparedInput) {
        outputFrameRate = preparedInput->video.frameRate;
    }
    if (plan.videoParameters.frameRate.complete() &&
        plan.videoParameters.frameRate.numerator &&
        plan.videoParameters.frameRate.denominator) {
        outputFrameRate = MediaRational{
            *plan.videoParameters.frameRate.numerator,
            *plan.videoParameters.frameRate.denominator};
    }
    std::optional<MediaProjectMpegTsResolvedPipelineFacts> resolvedTsFacts;
    if (MediaRealtimeRequestClassifier::muxedTransportOutput(options) &&
        options.parameters.execution.streamSet ==
            MediaTranscodeStreamSet::AudioVideo) {
        if (!plan.audioPlan || !plan.audioPlan->resolvedOutput) {
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
            *plan.audioPlan->resolvedOutput});
    }
    if (options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        if (!plan.audioPlan) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "AudioVideo planning requires an audio pipeline product"));
        }
        const MediaAudioPipelinePlan& plannedAudio = *plan.audioPlan;
        std::optional<MediaAvSyncPreparedDemuxTimestampFacts> demuxFacts;
        if (MediaRealtimeRequestClassifier::realtimeUrlInput(options)) {
            auto preparedFacts =
                preparedDemuxTimestampFacts(
                    plan, plannedAudio, preparedResource);
            if (!preparedFacts) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    preparedFacts.error());
            }
            demuxFacts = std::move(preparedFacts).value();
        }
        if (!plannedAudio.resolvedOutput) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V synchronization requires resolved output audio facts"));
        }
        const auto* selectedAudioVideoProgram = selectedTsProgram
            ? std::get_if<MediaTsAudioVideoSelectedProgramPlan>(
                  selectedTsProgram)
            : nullptr;
        auto avSync = MediaAvSyncPlanner::plan(
            options, selectedAudioVideoProgram,
            resolvedTsFacts ? &*resolvedTsFacts : nullptr,
            demuxFacts ? &*demuxFacts : nullptr,
            plannedAudio.branchMode,
            plannedAudio.resolvedOutput->sampleRate());
        if (!avSync) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(avSync.error());
        }
        auto componentBounds =
            MediaRealtimeAvSyncComponentBoundsPlanner::plan(
                plan.queues, plannedAudio);
        if (!componentBounds) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                componentBounds.error());
        }
        plan.avSyncComponentBounds = std::move(componentBounds).value();
        if (MediaRealtimeRequestClassifier::separateStreamsOutput(options)) {
            if (auto status = planScheduledRtpPacketization(
                    plan, plannedAudio, output, avSync.value()); !status) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    status.error());
            }
        }
        auto runtime = MediaRealtimeAvSyncRuntimePlanner::plan(
            plan, output, options, std::move(avSync).value(), outputFrameRate);
        if (!runtime) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                runtime.error());
        }
        MediaRealtimeRuntimePlan selectedRuntime(
            std::in_place_type<MediaRealtimeAvSyncRuntimePlan>,
            std::move(runtime).value());
        MediaRealtimeRtpTranscodePlan completed(
            std::move(plan), std::move(selectedRuntime));
        if (auto status = MediaRealtimeTsInputPlanValidator::validate(
                completed.inputType, completed.input); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                status.error());
        }
        if (auto status = validatePlannedProduct(completed); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                status.error());
        }
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(
            std::move(completed));
    } else {
        if (MediaRealtimeRequestClassifier::separateStreamsOutput(options)) {
            auto packetization = planVideoScheduledRtpPacketization(
                plan, output.videoOutput, 90'000, 96);
            if (!packetization) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    packetization.error());
            }
            output.videoOutput.scheduledPacketization =
                std::move(packetization).value();
        }
        MediaRational sourceTimeBase;
        if (rawInput) {
            sourceTimeBase = MediaRational{
                1, rawInput->videoTransport.clockRate};
        } else {
            if (!preparedResource) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "VideoOnly runtime requires its prepared video timing facts"));
            }
            const auto* videoSnapshot = preparedResource->inputStreamSnapshot(
                plan.videoPlan.sourceStreamIndex);
            if (videoSnapshot) {
                sourceTimeBase = videoSnapshot->time.timeBase;
            } else if (selectedTsProgram) {
                sourceTimeBase = std::visit(
                    [](const auto& selected) {
                        return selected.selection.video.timeBase;
                    },
                    *selectedTsProgram);
            }
        }
        auto runtime = MediaRealtimeVideoRuntimePlanner::plan(
            plan, std::move(output), options, sourceTimeBase,
            outputFrameRate);
        if (!runtime) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                runtime.error());
        }
        MediaRealtimeRuntimePlan selectedRuntime(
            std::in_place_type<MediaRealtimeVideoRuntimePlan>,
            std::move(runtime).value());
        MediaRealtimeRtpTranscodePlan completed(
            std::move(plan), std::move(selectedRuntime));
        if (auto status = MediaRealtimeTsInputPlanValidator::validate(
                completed.inputType, completed.input); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                status.error());
        }
        if (auto status = validatePlannedProduct(completed); !status) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                status.error());
        }
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(
            std::move(completed));
    }
}

::media::Result<MediaRealtimeRtpTranscodePlan>
MediaRealtimeRtpTranscodePlanner::planPreparedInput(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeInputStreamInfo& input,
    const MediaTsSelectedProgramPlan& selectedTsProgram)
{
    return planWithInput(
        request, &input, &selectedTsProgram, nullptr, nullptr, nullptr,
        std::nullopt, nullptr, nullptr);
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
        if (request.input.videoRtp.fmtp) {
            auto planned = plan(request);
            if (!planned) return ::media::Result<MediaRealtimeTranscodePreflight>::failure(planned.error());
            MediaRealtimeTranscodePreflight result(
                std::move(planned).value());
            return ::media::Result<MediaRealtimeTranscodePreflight>::success(std::move(result));
        }
        const auto preflightDeadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(*request.input.openTimeoutMs);
        auto remainingForProbe = remainingRawRtpStartupMilliseconds(
            preflightDeadline, "video signaling detection");
        if (!remainingForProbe) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                remainingForProbe.error());
        }
        MediaRealtimeRtpTranscodeRequest probeRequest = request;
        probeRequest.input.openTimeoutMs = remainingForProbe.value();
        auto probed = MediaRealtimeInputPlanner::prepareRawRtpVideo(
            probeRequest);
        if (!probed) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                probed.error());
        }
        auto remainingAfterProbe = remainingRawRtpStartupMilliseconds(
            preflightDeadline, "video signaling detection");
        if (!remainingAfterProbe) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                remainingAfterProbe.error());
        }
        const auto& detected = std::visit(
            [](const auto& prepared)
                -> const MediaDetectedRtpVideoSignaling& {
                return prepared.signaling;
            },
            probed.value());
        const auto& detectedFrameRate = std::visit(
            [](const auto& prepared) -> const MediaRational& {
                return prepared.sourceFrameRate;
            },
            probed.value());
        auto* videoOnlyProbe = std::get_if<
            MediaPreparedRawRtpVideoOnlyProbe>(&probed.value());
        auto* audioVideoProbe = std::get_if<
            MediaPreparedRawRtpAudioVideoProbe>(&probed.value());
        const bool expectsAudioVideo =
            request.parameters.execution.streamSet ==
            MediaTranscodeStreamSet::AudioVideo;
        if (expectsAudioVideo != (audioVideoProbe != nullptr)) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP prepared probe stream set conflicts with request"));
        }
        MediaPreparedRealtimeInput& preparedVideo = audioVideoProbe
            ? audioVideoProbe->video
            : videoOnlyProbe->video;
        MediaPreparedRealtimeInput* preparedAudio = audioVideoProbe
            ? &audioVideoProbe->audio
            : nullptr;
        auto preplannedVideo = planRawRtpVideoPipeline(
            request, detected, detectedFrameRate);
        if (!preplannedVideo) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                preplannedVideo.error());
        }
        if (auto status = preparedVideo.sealRawRtpPreflight();
            !status) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                status.error());
        }
        auto videoObservation = preparedVideo.rawRtpIngressObservation();
        if (!videoObservation) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                videoObservation.error());
        }
        auto socketCapacity =
            preparedVideo.rawRtpEffectiveSocketReceivePayloadBytes();
        if (!socketCapacity) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                socketCapacity.error());
        }
        auto videoCapability =
            MediaRtpIngressCapabilityMaterializer::materialize(
                socketCapacity.value());
        if (!videoCapability) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                videoCapability.error());
        }
        auto preparedByteCapacity =
            preparedVideo.rawRtpPreparedByteCapacity();
        if (!preparedByteCapacity) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                preparedByteCapacity.error());
        }
        auto videoIngress = MediaRtpIngressPlan::create(
            videoCapability.value(), videoObservation.value(),
            preparedByteCapacity.value());
        if (!videoIngress) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                videoIngress.error());
        }
        auto planned = planWithInput(
            request, nullptr, nullptr, &preparedVideo, preparedAudio,
            &videoIngress.value(), std::move(preplannedVideo).value(), &detected,
            &detectedFrameRate);
        if (!planned) return ::media::Result<MediaRealtimeTranscodePreflight>::failure(planned.error());
        auto remainingAfterPlanning = remainingRawRtpStartupMilliseconds(
            preflightDeadline, "resolved product planning");
        if (!remainingAfterPlanning) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                remainingAfterPlanning.error());
        }
        if (auto status = preparedVideo.configureRawRtpRuntimeIngress(
                videoIngress.value()); !status) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                status.error());
        }
        MediaRealtimeTranscodePreflight result(
            std::move(planned).value());
        MediaPreparedRawRtpProbe ownedProbe = std::move(probed).value();
        std::visit(
            [&result](auto&& prepared) {
                using Probe = std::decay_t<decltype(prepared)>;
                result.prepared.emplace(std::move(prepared.video));
                if constexpr (std::is_same_v<
                                  Probe,
                                  MediaPreparedRawRtpAudioVideoProbe>) {
                    result.preparedAudio.emplace(std::move(prepared.audio));
                }
            },
            std::move(ownedProbe));
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
        scan.selectedTsProgram ? &*scan.selectedTsProgram : nullptr,
        &scan.prepared, nullptr, nullptr, std::nullopt, nullptr, nullptr);
    if (!planned) {
        return ::media::Result<MediaRealtimeTranscodePreflight>::failure(planned.error());
    }
    MediaRealtimeTranscodePreflight result(
        std::move(planned).value());
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
    if (auto status = MediaRealtimeRtpInputPlanValidator::validate(
            plan.inputType, plan.input);
        !status) {
        return status;
    }
    const auto* avRuntime =
        std::get_if<MediaRealtimeAvSyncRuntimePlan>(&plan.runtime);
    if (avRuntime && avRuntime->isolatedAudioInput) {
        if (auto status = MediaRealtimeRtpInputPlanValidator::validate(
                RealtimeInputType::RtpPort,
                *avRuntime->isolatedAudioInput);
            !status) {
            return status;
        }
    }
    switch (plan.inputType) {
    case RealtimeInputType::RtpPort:
        if (plan.requiredPreparedInputKind &&
            *plan.requiredPreparedInputKind != MediaPreparedRealtimeInputKind::RawRtp) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP plan requires node-owned transport or prepared raw RTP input"));
        }
        if (!plan.input.requiresPreparedInput ||
            *plan.input.requiresPreparedInput !=
                plan.requiredPreparedInputKind.has_value()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP prepared ownership differs from planner product"));
        }
        break;
    case RealtimeInputType::Url:
        if (!plan.requiredPreparedInputKind ||
            *plan.requiredPreparedInputKind != MediaPreparedRealtimeInputKind::Generic) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "realtime URL plan requires prepared generic input"));
        }
        break;
    case RealtimeInputType::MpegTsUdp:
        if (!plan.requiredPreparedInputKind ||
            *plan.requiredPreparedInputKind != MediaPreparedRealtimeInputKind::MpegTs) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "MPEG-TS UDP plan requires prepared MPEG-TS input"));
        }
        break;
    default:
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "realtime plan input type is not supported"));
    }
    return MediaRealtimeRuntimePlanValidator::validate(plan);
}
} // namespace media::ffmpeg::graph

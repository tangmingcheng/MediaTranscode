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
#include "internal/graph/protocol/mpegts/MediaTsProgramContractValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoParameterSetValidator.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status planScheduledRtpPacketization(
    const MediaRealtimeRtpTranscodePlan& plan,
    MediaRealtimeOutputPlanningDraft& output,
    const MediaAvSyncPlan& synchronization)
{
    if (!synchronization.rtpOutput ||
        !synchronization.rtpOutput->videoOutput.clockRate ||
        !synchronization.rtpOutput->videoOutput.payloadType ||
        !synchronization.rtpOutput->audioOutput.clockRate ||
        !synchronization.rtpOutput->audioOutput.payloadType ||
        !plan.audioPlan.resolvedOutput || output.videoOutput.packetSize <= 0 ||
        output.audioOutput.packetSize <= 0) {
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
        *synchronization.rtpOutput->videoOutput.clockRate,
        *synchronization.rtpOutput->videoOutput.payloadType,
        static_cast<std::size_t>(output.videoOutput.packetSize));
    auto audio = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, plan.audioPlan.resolvedOutput->codecName(), 1,
        *synchronization.rtpOutput->audioOutput.clockRate,
        *synchronization.rtpOutput->audioOutput.payloadType,
        static_cast<std::size_t>(output.audioOutput.packetSize),
        plan.audioPlan.resolvedOutput->codecFrameSamples());
    if (!video || !audio) {
        return ::media::Status::failure(video ? audio.error() : video.error());
    }
    output.videoOutput.scheduledPacketization = std::move(video).value();
    output.audioOutput.scheduledPacketization = std::move(audio).value();
    return ::media::Status::success();
}

::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>
preparedDemuxTimestampFacts(
    const MediaRealtimeRtpTranscodePlan& plan,
    const MediaPreparedRealtimeInput* prepared)
{
    if (!prepared) {
        return ::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "URL A/V planning requires the prepared input snapshot"));
    }
    const auto* video =
        prepared->inputStreamSnapshot(plan.videoPlan.sourceStreamIndex);
    const auto* audio =
        prepared->inputStreamSnapshot(plan.audioPlan.sourceStreamIndex);
    if (!video || !audio || video->index < 0 || audio->index < 0 ||
        video->index == audio->index || !video->time.timeBase.isKnown() ||
        video->time.timeBase.num <= 0 || video->time.timeBase.den <= 0 ||
        !audio->time.timeBase.isKnown() || audio->time.timeBase.num <= 0 ||
        audio->time.timeBase.den <= 0) {
        return ::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>::failure(
            ::media::ErrorInfo::notInitialized(
                "URL A/V planning requires explicit prepared stream time bases"));
    }
    return ::media::Result<MediaAvSyncPreparedDemuxTimestampFacts>::success(
        MediaAvSyncPreparedDemuxTimestampFacts{
            video->index, video->time.timeBase,
            audio->index, audio->time.timeBase});
}

MediaRealtimeSingleStreamOutputPlan singleStreamOutput(
    MediaRealtimeOutputPlanningDraft output)
{
    return MediaRealtimeSingleStreamOutputPlan{
        output.packetCopyNormalizationRequired,
        std::move(output.videoOutput),
        std::move(output.muxedOutput),
        std::move(output.sdp),
        std::move(output.singleStreamMux)};
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

    MediaAudioPipelinePlannerOptions plannerOptions(
        *options.parameters.execution.streamSet);
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
        options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo;
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

::media::Result<MediaPipelinePlan> planRawRtpVideoPipeline(
    const MediaRealtimeRtpTranscodeRequest& request)
{
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
    MediaInputVideoStreamInfo input;
    input.streamIndex = MediaRealtimeRawInputPlan::VideoStreamIndex;
    input.codecName = canonicalCodecName(request.input.videoRtp.codecName);
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

::media::Result<MediaRealtimeRtpTranscodeRequest>
resolveManualRawRtpVideoSignaling(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!MediaRealtimeRequestClassifier::rawRtpInput(request) ||
        !request.input.videoRtp.fmtp) {
        return ::media::Result<MediaRealtimeRtpTranscodeRequest>::success(
            request);
    }
    auto facts = parseRtpVideoSignalingFacts(
        request.input.videoRtp.codecName, *request.input.videoRtp.fmtp);
    if (!facts) {
        return ::media::Result<MediaRealtimeRtpTranscodeRequest>::failure(
            facts.error());
    }
    if (auto status = MediaRtpVideoParameterSetValidator::validate(
            request.input.videoRtp.codecName, facts.value());
        !status) {
        return ::media::Result<MediaRealtimeRtpTranscodeRequest>::failure(
            status.error());
    }
    auto fmtp = serializeRtpVideoFmtp(facts.value());
    if (!fmtp) {
        return ::media::Result<MediaRealtimeRtpTranscodeRequest>::failure(
            fmtp.error());
    }
    MediaRealtimeRtpTranscodeRequest resolved = request;
    resolved.input.videoRtp.fmtp = std::move(fmtp).value();
    return ::media::Result<MediaRealtimeRtpTranscodeRequest>::success(
        std::move(resolved));
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
    const auto packetCapacity = request.parameters.queues.packet;
    if (packetCapacity == 0 ||
        !request.avSyncStartup.maximumVideoUnitBytes ||
        *request.avSyncStartup.maximumVideoUnitBytes == 0) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS retention requires explicit video bounds"));
    }
    const auto videoUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumVideoUnitBytes);
    if (packetCapacity > std::numeric_limits<std::uint64_t>::max() /
            videoUnitBytes) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video retention capacity is not representable"));
    }
    const auto videoByteCapacity =
        static_cast<std::uint64_t>(packetCapacity) * videoUnitBytes;
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
    if (!request.avSyncStartup.maximumAudioUnitBytes ||
        *request.avSyncStartup.maximumAudioUnitBytes == 0) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::notInitialized(
                "AudioVideo MPEG-TS retention requires explicit audio bounds"));
    }
    const auto audioUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumAudioUnitBytes);
    if (packetCapacity > std::numeric_limits<std::uint64_t>::max() /
            audioUnitBytes) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS audio retention capacity is not representable"));
    }
    const auto audioByteCapacity =
        static_cast<std::uint64_t>(packetCapacity) * audioUnitBytes;
    if (audioByteCapacity > maximumSerialized) {
        return ::media::Result<MediaRealtimeTsInputPlan::Retention>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS retention exceeds runtime option range"));
    }
    return ::media::Result<MediaRealtimeTsInputPlan::Retention>::success(
        MediaRealtimeTsInputPlan::AudioVideoRetention(
            packetCapacity, packetCapacity,
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
    return planWithInput(options, nullptr, nullptr, nullptr, nullptr);
}

::media::Result<MediaRealtimeRtpTranscodePlan> MediaRealtimeRtpTranscodePlanner::planWithInput(
    const MediaRealtimeRtpTranscodeRequest& requestedOptions,
    const MediaRealtimeInputStreamInfo* preparedInput,
    const MediaTsSelectedProgramPlan* selectedTsProgram,
    const MediaPreparedRealtimeInput* preparedResource,
    const MediaPreparedRealtimeInput* preparedAudioResource,
    std::optional<MediaPipelinePlan> preplannedVideo)
{
    if (auto status = validateRealtimeRequestNoIo(requestedOptions); !status) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(status.error());
    }
    auto resolvedOptions = resolveManualRawRtpVideoSignaling(requestedOptions);
    if (!resolvedOptions) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
            resolvedOptions.error());
    }
    MediaRealtimeRtpTranscodeRequest options = std::move(resolvedOptions).value();
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
    MediaAudioPipelinePlan audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    if (MediaRealtimeRequestClassifier::rawRtpInput(options)) {
        auto raw = MediaRealtimeInputPlanner::planRawRtp(
            options,
            plannedRawRtpAvSync ? &*plannedRawRtpAvSync : nullptr);
        if (!raw) return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(raw.error());
        rawInput.emplace(std::move(raw).value());

        if (options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
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
        if (plan.useIsolatedAudioInput) {
            if (*plan.input.requiresPreparedInput &&
                !preparedAudioResource) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "raw RTP automatic signaling requires synchronized prepared audio input"));
            }
            plan.audioInput.requiresPreparedInput =
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
    std::optional<MediaProjectMpegTsResolvedPipelineFacts> resolvedTsFacts;
    if (MediaRealtimeRequestClassifier::muxedTransportOutput(options)) {
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
    if (options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo) {
        std::optional<MediaAvSyncPreparedDemuxTimestampFacts> demuxFacts;
        if (MediaRealtimeRequestClassifier::realtimeUrlInput(options)) {
            auto preparedFacts =
                preparedDemuxTimestampFacts(plan, preparedResource);
            if (!preparedFacts) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    preparedFacts.error());
            }
            demuxFacts = std::move(preparedFacts).value();
        }
        if (!plan.audioPlan.resolvedOutput) {
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
            plan.audioPlan.resolvedOutput->sampleRate());
        if (!avSync) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(avSync.error());
        }
        auto componentBounds =
            MediaRealtimeAvSyncComponentBoundsPlanner::plan(plan);
        if (!componentBounds) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                componentBounds.error());
        }
        plan.avSyncComponentBounds = std::move(componentBounds).value();
        if (MediaRealtimeRequestClassifier::separateStreamsOutput(options)) {
            if (auto status = planScheduledRtpPacketization(
                    plan, output, avSync.value()); !status) {
                return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                    status.error());
            }
        }
        auto runtime = MediaRealtimeAvSyncRuntimePlanner::plan(
            plan, output, std::move(avSync).value());
        if (!runtime) {
            return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(
                runtime.error());
        }
        plan.avSyncRuntime = std::move(runtime).value();
    } else {
        plan.singleStreamOutput.emplace(singleStreamOutput(std::move(output)));
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
    return planWithInput(
        request, &input, &selectedTsProgram, nullptr, nullptr);
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
            MediaRealtimeTranscodePreflight result;
            result.plan = std::move(planned).value();
            return ::media::Result<MediaRealtimeTranscodePreflight>::success(std::move(result));
        }
        const auto preflightDeadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(*request.input.openTimeoutMs);
        auto preplannedVideo = planRawRtpVideoPipeline(request);
        if (!preplannedVideo) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                preplannedVideo.error());
        }
        auto remainingForProbe = remainingRawRtpStartupMilliseconds(
            preflightDeadline, "hardware pipeline validation");
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
        if (!request.input.videoRtp.payloadType ||
            !request.input.videoRtp.clockRate ||
            detected.codecName != canonicalCodecName(
                request.input.videoRtp.codecName) ||
            detected.payloadType != *request.input.videoRtp.payloadType ||
            detected.clockRate != *request.input.videoRtp.clockRate) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP detected signaling identity conflicts with request"));
        }
        if (auto status = MediaRtpVideoParameterSetValidator::validate(
                detected.codecName, detected.facts); !status) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                status.error());
        }
        auto fmtp = serializeRtpVideoFmtp(detected.facts);
        if (!fmtp) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                fmtp.error());
        }
        MediaRealtimeRtpTranscodeRequest resolved = request;
        resolved.input.videoRtp.fmtp = std::move(fmtp).value();
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
        auto planned = planWithInput(
            resolved, nullptr, nullptr, &preparedVideo, preparedAudio,
            std::move(preplannedVideo).value());
        if (!planned) return ::media::Result<MediaRealtimeTranscodePreflight>::failure(planned.error());
        auto remainingAfterPlanning = remainingRawRtpStartupMilliseconds(
            preflightDeadline, "resolved product planning");
        if (!remainingAfterPlanning) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                remainingAfterPlanning.error());
        }
        if (auto status = preparedVideo.sealRawRtpPreflight();
            !status) {
            return ::media::Result<MediaRealtimeTranscodePreflight>::failure(
                status.error());
        }
        MediaRealtimeTranscodePreflight result;
        result.plan = std::move(planned).value();
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
        &scan.prepared, nullptr);
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
    switch (plan.inputType) {
    case RealtimeInputType::RtpPort:
        if (plan.requiredPreparedInputKind &&
            *plan.requiredPreparedInputKind != MediaPreparedRealtimeInputKind::RawRtp) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP plan requires node-owned transport or prepared raw RTP input"));
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
    return MediaRealtimeAvSyncRuntimePlanValidator::validate(plan);
}
} // namespace media::ffmpeg::graph

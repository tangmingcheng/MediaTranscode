#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"

#include "internal/graph/planner/MediaRtpClockLivenessPolicy.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupLimits.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <cstdint>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t Millisecond = 1'000'000;
constexpr std::int64_t Second = 1'000'000'000;

constexpr MediaRunningTime runningTime(std::int64_t nanoseconds) noexcept
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

::media::Status planSharedPolicy(MediaAvSyncPlan& plan,
                                 const MediaRealtimeRtpTranscodeRequest& request)
{
    if (request.parameters.queues.packet == 0 ||
        request.parameters.queues.packet > MediaAvStartupMaximumUnitCapacity ||
        !request.avSyncStartup.maximumVideoUnitBytes ||
        !request.avSyncStartup.maximumAudioUnitBytes ||
        !request.avSyncStartup.maximumGap ||
        *request.avSyncStartup.maximumVideoUnitBytes == 0 ||
        *request.avSyncStartup.maximumAudioUnitBytes == 0 ||
        *request.avSyncStartup.maximumGap <= runningTime(0)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup requires explicit unit and byte capacity inputs"));
    }
    plan.masterClockMode = MediaAvSyncMasterClockMode::SteadyMonotonic;
    plan.canonicalTimeBaseNumerator = 1;
    plan.canonicalTimeBaseDenominator = 1'000'000'000;

    plan.startup.requireVideoKeyFrame = true;
    plan.startup.trimAudioToCommonStart = true;
    plan.startup.maximumWaitNs = runningTime(10 * Second);
    plan.startup.prerollNs = runningTime(500 * Millisecond);
    plan.startup.keyFrameWaitNs = runningTime(5 * Second);
    plan.startup.maximumAudioTrimNs = runningTime(250 * Millisecond);
    plan.startup.maximumInitialSkewNs = runningTime(40 * Millisecond);
    plan.startup.maximumGapNs = *request.avSyncStartup.maximumGap;
    plan.startup.outputLeadNs = runningTime(100 * Millisecond);
    plan.startup.videoCapacity = request.parameters.queues.packet;
    plan.startup.audioCapacity = request.parameters.queues.packet;
    const auto units = static_cast<std::uint64_t>(request.parameters.queues.packet);
    const auto videoUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumVideoUnitBytes);
    const auto audioUnitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumAudioUnitBytes);
    if (units > std::numeric_limits<std::uint64_t>::max() / videoUnitBytes ||
        units > std::numeric_limits<std::uint64_t>::max() / audioUnitBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup byte capacity is not representable"));
    }
    const auto videoByteCapacity = units * videoUnitBytes;
    const auto audioByteCapacity = units * audioUnitBytes;
    const auto maximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (videoUnitBytes > maximumSerialized || audioUnitBytes > maximumSerialized ||
        videoByteCapacity > maximumSerialized || audioByteCapacity > maximumSerialized) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup capacity exceeds the runtime option range"));
    }
    plan.startup.videoByteCapacity = videoByteCapacity;
    plan.startup.audioByteCapacity = audioByteCapacity;
    plan.startup.maximumVideoUnitBytes = videoUnitBytes;
    plan.startup.maximumAudioUnitBytes = audioUnitBytes;
    plan.startup.allowDegradedClock = false;

    plan.audioServo.deadbandNs = runningTime(Millisecond);
    plan.audioServo.phaseFilterTimeConstantNs = runningTime(250 * Millisecond);
    plan.audioServo.proportionalGainPpmPerSecond = 20'000;
    plan.audioServo.integralGainPpmPerSecondSquared = 1'000;
    plan.audioServo.integratorLimitPpm = 2'000;
    plan.audioServo.frequencyFeedForwardNumerator = 1;
    plan.audioServo.frequencyFeedForwardDenominator = 1;
    plan.audioServo.frequencyDeadbandPpm = 10;
    plan.audioServo.maximumMeasuredFrequencyPpm = 10'000;
    plan.audioServo.recoveryExitFrequencyPpm = 500;
    plan.audioServo.antiWindupMode =
        MediaAudioServoAntiWindupMode::ConditionalIntegration;
    plan.audioServo.minimumUpdateIntervalNs = runningTime(10 * Millisecond);
    plan.audioServo.maximumMeasurementGapNs = runningTime(Second);
    plan.audioServo.maximumSlewPpmPerSecond = 100;
    plan.audioServo.normalCorrectionLimitPpm = 1000;
    plan.audioServo.recoveryCorrectionLimitPpm = 5000;
    plan.audioServo.recoveryEnterThresholdNs = runningTime(100 * Millisecond);
    plan.audioServo.recoveryExitThresholdNs = runningTime(50 * Millisecond);
    plan.audioServo.recoveryExitHoldNs = runningTime(500 * Millisecond);
    plan.audioServo.correctionLookaheadWindows = 2;

    plan.video.earlyHoldThresholdNs = runningTime(20 * Millisecond);
    plan.video.lateDisplayThresholdNs = runningTime(40 * Millisecond);
    plan.video.dropThresholdNs = runningTime(80 * Millisecond);
    plan.video.allowRecoveryRepeat = true;
    plan.video.maximumConsecutiveRecoveryActions = 5;

    plan.recovery.suspectThresholdNs = runningTime(100 * Millisecond);
    plan.recovery.hardDiscontinuityThresholdNs = runningTime(250 * Millisecond);
    plan.recovery.reacquisitionTimeoutNs = runningTime(10 * Second);

    plan.metrics.collectStateAndGeneration = true;
    plan.metrics.collectClockEvidence = true;
    plan.metrics.collectQueueDurations = true;
    plan.metrics.collectPhaseErrors = true;
    plan.metrics.collectAudioCorrection = true;
    plan.metrics.collectVideoRecoveryCounts = true;
    plan.metrics.collectDiscontinuityCounts = true;
    plan.metrics.collectProtocolClockHealth = true;
    plan.metrics.maximumStartupSkewNs = runningTime(40 * Millisecond);
    plan.metrics.maximumSteadyP95SkewNs = runningTime(20 * Millisecond);
    plan.metrics.maximumSteadyP99SkewNs = runningTime(40 * Millisecond);
    plan.metrics.maximumDriftNsPerHour = runningTime(Millisecond);
    return ::media::Status::success();
}

::media::Result<MediaAvSyncRtpInputPlan> planRtpInput(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.input.videoRtp.payloadType || !request.input.videoRtp.clockRate ||
        !request.input.audioRtp.payloadType || !request.input.audioRtp.clockRate) {
        return ::media::Result<MediaAvSyncRtpInputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized separate RTP requires explicit audio/video payload types and clock rates"));
    }

    MediaAvSyncRtpInputPlan input;
    const std::string groupIdentity = request.mediaId.empty()
        ? std::string("realtime-av-sync")
        : request.mediaId;

    input.videoInput.identity = groupIdentity + ".input.video";
    input.videoInput.payloadType = *request.input.videoRtp.payloadType;
    input.videoInput.clockRate = *request.input.videoRtp.clockRate;
    input.audioInput.identity = groupIdentity + ".input.audio";
    input.audioInput.payloadType = *request.input.audioRtp.payloadType;
    input.audioInput.clockRate = *request.input.audioRtp.clockRate;
    input.input.streamAssociationMode =
        MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair;
    input.input.rtcpCompositionMode =
        MediaRtcpCompositionMode::ReducedSizeRfc5506;
    input.input.identityEvidenceTimeoutNs = runningTime(
        static_cast<std::int64_t>(MediaRtpClockLivenessPolicy::CnameTimeoutMs) *
        Millisecond);
    input.input.commonEpochPolicy =
        MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime;
    input.input.requireSenderReports = true;
    input.input.senderReportTimeoutNs = runningTime(
        static_cast<std::int64_t>(
            MediaRtpClockLivenessPolicy::SenderReportTimeoutMs) *
        Millisecond);
    input.input.maximumExtrapolationNs = runningTime(
        static_cast<std::int64_t>(
            MediaRtpClockLivenessPolicy::MaximumExtrapolationMs) *
        Millisecond);
    input.input.maximumInterStreamClockOffsetSkewNs =
        runningTime(50 * Millisecond);
    input.input.maximumSenderClockRateErrorPpm = 1'000;
    input.input.maximumSenderClockResidualNs = runningTime(250 * Millisecond);
    return ::media::Result<MediaAvSyncRtpInputPlan>::success(std::move(input));
}

void planRtpOutput(MediaAvSyncPlan& plan,
                   const MediaRealtimeRtpTranscodeRequest& request,
                   int audioOutputRate)
{
    const std::string groupIdentity = request.mediaId.empty()
        ? std::string("realtime-av-sync")
        : request.mediaId;
    const std::string cname =
        MediaRtpOutputIdentityPlanner::cname(groupIdentity);
    plan.rtpOutput.emplace();
    plan.rtpOutput->videoOutput.identity = groupIdentity + ".output.video";
    plan.rtpOutput->videoOutput.payloadType = 96;
    plan.rtpOutput->videoOutput.clockRate = 90'000;
    plan.rtpOutput->videoOutput.ssrc =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            *plan.rtpOutput->videoOutput.identity);
    plan.rtpOutput->videoOutput.baseTimestamp =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            groupIdentity + ".video.timestamp");
    plan.rtpOutput->videoOutput.cname = cname;
    plan.rtpOutput->audioOutput.identity = groupIdentity + ".output.audio";
    plan.rtpOutput->audioOutput.payloadType = 97;
    plan.rtpOutput->audioOutput.clockRate = audioOutputRate;
    plan.rtpOutput->audioOutput.ssrc =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            *plan.rtpOutput->audioOutput.identity);
    plan.rtpOutput->audioOutput.baseTimestamp =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            groupIdentity + ".audio.timestamp");
    plan.rtpOutput->audioOutput.cname = cname;
    plan.rtpOutput->output.useSharedNtpEpoch = true;
    plan.rtpOutput->output.senderReportIntervalNs = runningTime(Second);
}

void planTsInput(MediaAvSyncPlan& plan,
                 const MediaRealtimeRtpTranscodeRequest& request,
                 const MediaTsSelectedProgramPlan& selected)
{
    plan.sourceClockMode = MediaAvSyncSourceClockMode::MpegTsPcr;
    plan.controlGenerationPolicy =
        MediaControlGenerationPolicy::OptionalExactWhenPresent;
    plan.mpegTsInput.emplace();
    plan.mpegTsInput->programNumber = selected.programNumber;
    plan.mpegTsInput->programMapPid = selected.programMapPid;
    plan.mpegTsInput->videoPid = selected.videoPid;
    plan.mpegTsInput->audioPid = selected.audioPid;
    const std::string groupIdentity = request.mediaId.empty()
        ? std::string("realtime-av-sync-ts")
        : request.mediaId;
    plan.startup.videoIdentity = groupIdentity + ".pid." +
                                 std::to_string(selected.videoPid);
    plan.startup.audioIdentity = groupIdentity + ".pid." +
                                 std::to_string(selected.audioPid);
    plan.mpegTsInput->pcrPid = selected.pcrPid;
}

::media::Result<MediaTsMuxPlan> planTsOutput(
    const MediaAvSyncPlan& plan,
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaProjectMpegTsResolvedPipelineFacts& resolvedFacts)
{
    if (!plan.startup.outputLeadNs) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires planner-owned startup output lead"));
    }
    if (!request.output.transport) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires an explicit transport"));
    }
    std::uint8_t maximumPacketsPerDatagram = 7;
    if (*request.output.transport == MediaOutputTransportKind::RtpAvp) {
        if (!request.output.packetSize ||
            *request.output.packetSize <= 0) {
            return ::media::Result<MediaTsMuxPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS RTP output requires an explicit maximum datagram size"));
        }
        auto packetCount = MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
            static_cast<std::size_t>(*request.output.packetSize));
        if (!packetCount) {
            return ::media::Result<MediaTsMuxPlan>::failure(
                packetCount.error());
        }
        maximumPacketsPerDatagram = packetCount.value();
    } else if (*request.output.transport !=
               MediaOutputTransportKind::UdpDatagrams) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "MPEG-TS output transport is unsupported"));
    }
    auto resolvedOutput = MediaProjectMpegTsOutputPlan::create(
        resolvedFacts.videoCodecName, resolvedFacts.videoPacketLayout,
        resolvedFacts.audioOutput,
        *plan.startup.outputLeadNs,
        *request.output.transport,
        maximumPacketsPerDatagram);
    if (!resolvedOutput) {
        return ::media::Result<MediaTsMuxPlan>::failure(resolvedOutput.error());
    }
    return ::media::Result<MediaTsMuxPlan>::success(
        resolvedOutput.value().muxPlan());
}

} // namespace

::media::Result<MediaAvSyncRtpInputPlan> MediaAvSyncPlanner::planRtpInputClock(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    return planRtpInput(request);
}

::media::Result<MediaAvSyncPlan> MediaAvSyncPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaTsSelectedProgramPlan* selectedTsProgram,
    const MediaProjectMpegTsResolvedPipelineFacts* resolvedTsFacts,
    const MediaAvSyncPreparedDemuxTimestampFacts* preparedDemuxFacts,
    int resolvedOutputAudioSampleRate)
{
    if (!request.parameters.execution.includeAudio) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported("A/V synchronization requires both audio and video"));
    }
    if (resolvedOutputAudioSampleRate <= 0) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V synchronization requires a resolved output audio sample rate"));
    }
    MediaAvSyncPlan plan;
    if (auto status = planSharedPolicy(plan, request); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    plan.audioServo.outputSampleRate = resolvedOutputAudioSampleRate;

    if (MediaRealtimeRequestClassifier::rawRtpInput(request)) {
        auto rtpInput = planRtpInput(request);
        if (!rtpInput) {
            return ::media::Result<MediaAvSyncPlan>::failure(rtpInput.error());
        }
        plan.sourceClockMode = MediaAvSyncSourceClockMode::RtpSenderReports;
        plan.controlGenerationPolicy =
            MediaControlGenerationPolicy::OptionalExactWhenPresent;
        plan.rtpInput = std::move(rtpInput).value();
        plan.startup.videoIdentity = plan.rtpInput->videoInput.identity;
        plan.startup.audioIdentity = plan.rtpInput->audioInput.identity;
        if (selectedTsProgram || preparedDemuxFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP input clock rejects MPEG-TS and demux input facts"));
        }
    } else if (MediaRealtimeRequestClassifier::mpegTsUdpInput(request)) {
        if (!selectedTsProgram) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS A/V synchronization requires planner-selected program identity"));
        }
        if (preparedDemuxFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS input clock rejects demux timestamp facts"));
        }
        planTsInput(plan, request, *selectedTsProgram);
    } else if (MediaRealtimeRequestClassifier::realtimeUrlInput(request)) {
        if (selectedTsProgram || !preparedDemuxFacts ||
            preparedDemuxFacts->videoStreamIndex < 0 ||
            preparedDemuxFacts->audioStreamIndex < 0 ||
            preparedDemuxFacts->videoStreamIndex ==
                preparedDemuxFacts->audioStreamIndex ||
            !preparedDemuxFacts->videoTimeBase.isKnown() ||
            preparedDemuxFacts->videoTimeBase.num <= 0 ||
            preparedDemuxFacts->videoTimeBase.den <= 0 ||
            !preparedDemuxFacts->audioTimeBase.isKnown() ||
            preparedDemuxFacts->audioTimeBase.num <= 0 ||
            preparedDemuxFacts->audioTimeBase.den <= 0) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "URL A/V input clock requires prepared stream time bases"));
        }
        plan.sourceClockMode = MediaAvSyncSourceClockMode::DemuxTimestamps;
        plan.controlGenerationPolicy =
            MediaControlGenerationPolicy::RequiredExact;
        const std::string groupIdentity = request.mediaId.empty()
            ? std::string("realtime-av-sync-demux")
            : request.mediaId;
        plan.startup.videoIdentity = groupIdentity + ".stream." +
            std::to_string(preparedDemuxFacts->videoStreamIndex);
        plan.startup.audioIdentity = groupIdentity + ".stream." +
            std::to_string(preparedDemuxFacts->audioStreamIndex);
        plan.demuxTimestampInput.emplace(
            MediaAvSyncDemuxTimestampInputPlan{
                preparedDemuxFacts->videoTimeBase,
                preparedDemuxFacts->audioTimeBase,
                plan.startup.maximumInitialSkewNs,
                plan.recovery.hardDiscontinuityThresholdNs,
                1,
                MediaRunningTime::fromNanoseconds(0)});
    } else {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Realtime A/V input clock is not supported"));
    }

    if (MediaRealtimeRequestClassifier::separateStreamsOutput(request)) {
        if (!MediaRealtimeRequestClassifier::rtpAvpOutput(request) ||
            resolvedTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Separate RTP output rejects MPEG-TS output facts"));
        }
        planRtpOutput(plan, request, resolvedOutputAudioSampleRate);
    } else if (MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        if (!resolvedTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output requires resolved H.264/AAC pipeline facts"));
        }
        auto outputMux = planTsOutput(plan, request, *resolvedTsFacts);
        if (!outputMux) {
            return ::media::Result<MediaAvSyncPlan>::failure(outputMux.error());
        }
        const bool useSharedNtpEpoch =
            outputMux.value().parameters().transportKind ==
            MediaOutputTransportKind::RtpAvp;
        plan.projectMpegTsOutput.emplace();
        plan.projectMpegTsOutput->outputMux = std::move(outputMux).value();
        plan.projectMpegTsOutput->useSharedNtpEpoch = useSharedNtpEpoch;
    } else {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Realtime A/V output adapter is not supported"));
    }

    if (auto status = MediaAvSyncPlanValidator::validatePolicy(plan); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph

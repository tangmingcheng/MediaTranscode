#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeDatagramTransportPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaScheduledDatagramPacingPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <optional>
#include <limits>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;

::media::Result<MediaRealtimeEdgePolicySet> planBoundedEdgePolicies(
    const MediaRealtimeRtpTranscodePlanningDraft& outer,
    const MediaAvSyncPlan& synchronization)
{
    if (!synchronization.startup.videoByteCapacity ||
        !synchronization.startup.audioByteCapacity ||
        *synchronization.startup.videoByteCapacity == 0 ||
        *synchronization.startup.audioByteCapacity == 0 ||
        outer.queues.packet == 0 ||
        *synchronization.startup.videoByteCapacity >
            (std::numeric_limits<std::uint64_t>::max)() -
                *synchronization.startup.audioByteCapacity) {
        return ::media::Result<MediaRealtimeEdgePolicySet>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V edge byte capacity is incomplete or not representable"));
    }
    const auto maximumBytes =
        *synchronization.startup.videoByteCapacity +
        *synchronization.startup.audioByteCapacity;
    return MediaRealtimeEdgePolicyPlanner::
        planWithSynchronizedPacketMemoryBudget(
            outer.queues, maximumBytes, outer.queues.packet);
}

::media::Result<MediaRealtimeAvSyncAssemblyPlan> planAssembly(
    const MediaRealtimeRtpTranscodePlanCore& outer,
    const MediaAudioPipelinePlan& audio,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSyncPlanningFacts& facts)
{
    if (!audio.enabled || !synchronization.sourceClockMode ||
        !synchronization.startup.videoIdentity ||
        synchronization.startup.videoIdentity->empty() ||
        !synchronization.startup.audioIdentity ||
        synchronization.startup.audioIdentity->empty() ||
        !synchronization.startup.videoCapacity ||
        *synchronization.startup.videoCapacity == 0 ||
        !synchronization.startup.audioCapacity ||
        *synchronization.startup.audioCapacity == 0 ||
        !synchronization.startup.maximumWaitNs ||
        *synchronization.startup.maximumWaitNs <=
            MediaRunningTime::fromNanoseconds(0) ||
        !synchronization.audioServo.minimumUpdateIntervalNs ||
        *synchronization.audioServo.minimumUpdateIntervalNs <=
            MediaRunningTime::fromNanoseconds(0) ||
        facts.inputVideoIdentity != synchronization.startup.videoIdentity ||
        facts.inputAudioIdentity != synchronization.startup.audioIdentity) {
        return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V production assembly requires complete startup and source facts"));
    }

    MediaAvSyncInputClockPlan inputClock;
    MediaCanonicalVideoDurationPlan videoDuration;
    MediaCanonicalAudioDurationPlan audioDuration;
    std::uint64_t initialGeneration = MediaFirstLockedSourceGeneration;
    if (*synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (outer.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
            !synchronization.rtpInput || !facts.inputVideoClockRate ||
            *facts.inputVideoClockRate <= 0 || !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0 ||
            (outer.videoPlan.inputCodecName != "h264" &&
             outer.videoPlan.inputCodecName != "hevc")) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaRtpInputClockAssemblyPlan>(
            synchronization.rtpInput->input.commonEpochPolicy);
        videoDuration.emplace<MediaRtpTimestampDeltaDurationPlan>(
            *facts.inputVideoClockRate,
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        if (outer.inputType != RealtimeInputType::MpegTsUdp ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::MuxedTransportStream ||
            !synchronization.mpegTsInput || !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0 ||
            !facts.inputVideoPacketDuration ||
            facts.inputVideoPacketDuration->packetDuration <= 0 ||
            facts.inputVideoPacketDuration->timeBase.num <= 0 ||
            facts.inputVideoPacketDuration->timeBase.den <= 0 ||
            !facts.inputAudioPacketDuration ||
            facts.inputAudioPacketDuration->packetDuration <= 0 ||
            facts.inputAudioPacketDuration->timeBase.num <= 0 ||
            facts.inputAudioPacketDuration->timeBase.den <= 0) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaMpegTsInputClockAssemblyPlan>();
        videoDuration.emplace<MediaPacketDurationPlan>(true);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (outer.inputType != RealtimeInputType::Url ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::SessionDescribed ||
            !synchronization.demuxTimestampInput ||
            !synchronization.demuxTimestampInput->firstWindowMaximumSkewNs ||
            !synchronization.demuxTimestampInput->discontinuityThresholdNs ||
            !synchronization.demuxTimestampInput->initialGeneration ||
            !synchronization.demuxTimestampInput->canonicalTargetEpochNs ||
            !synchronization.demuxTimestampInput->preparedInput ||
            !synchronization.demuxTimestampInput->preparedEvidence ||
            !synchronization.demuxTimestampInput->videoTimeBase.isKnown() ||
            synchronization.demuxTimestampInput->videoTimeBase.num <= 0 ||
            synchronization.demuxTimestampInput->videoTimeBase.den <= 0 ||
            !synchronization.demuxTimestampInput->audioTimeBase.isKnown() ||
            synchronization.demuxTimestampInput->audioTimeBase.num <= 0 ||
            synchronization.demuxTimestampInput->audioTimeBase.den <= 0 ||
            !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp production assembly facts are incomplete"));
        }
        const auto& demux = *synchronization.demuxTimestampInput;
        initialGeneration = *demux.initialGeneration;
        inputClock.emplace<MediaDemuxTimestampInputClockAssemblyPlan>(
            MediaDemuxTimestampInputClockAssemblyPlan{
                demux.videoTimeBase,
                demux.audioTimeBase,
                *demux.firstWindowMaximumSkewNs,
                *demux.discontinuityThresholdNs,
                initialGeneration,
                *synchronization.startup.videoIdentity,
                *synchronization.startup.audioIdentity,
                *demux.canonicalTargetEpochNs,
                *demux.preparedInput,
                *demux.preparedEvidence});
        videoDuration.emplace<MediaPacketDurationPlan>(true);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else {
        return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V production input clock mode is unsupported"));
    }

    return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::success(
        MediaRealtimeAvSyncAssemblyPlan{
            std::move(inputClock),
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange,
            initialGeneration,
            MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire,
            MediaCanonicalVideoAssemblyPlan{
                *synchronization.startup.videoIdentity,
                std::move(videoDuration),
                MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
                *synchronization.startup.videoCapacity,
                *synchronization.startup.maximumWaitNs},
            MediaCanonicalAudioAssemblyPlan{
                *synchronization.startup.audioIdentity,
                std::move(audioDuration),
                MediaDecodeOrderMode::PresentationOrderNoReorder,
                *synchronization.startup.audioCapacity,
                *synchronization.startup.maximumWaitNs},
            *synchronization.audioServo.minimumUpdateIntervalNs});
}

::media::Result<MediaScheduledRtpOutputPlan> scheduledRtpOutput(
    MediaScheduledStream stream,
    MediaRealtimeScheduledRtpOutputPlanningDraft& plannedOutput,
    const MediaAvSyncRtpOutputStreamPlan& synchronization,
    MediaRunningTime senderLead,
    MediaRunningTime senderReportInterval)
{
    if (!synchronization.payloadType || !synchronization.ssrc ||
        !synchronization.baseTimestamp || !synchronization.clockRate ||
        !synchronization.cname || synchronization.cname->empty() ||
        plannedOutput.packetSize <= 0 || !plannedOutput.scheduledTransport ||
        !plannedOutput.scheduledPacketization) {
        return ::media::Result<MediaScheduledRtpOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled RTP output requires complete protocol planning facts"));
    }
    return ::media::Result<MediaScheduledRtpOutputPlan>::success(
        MediaScheduledRtpOutputPlan{
            stream,
            std::move(*plannedOutput.scheduledTransport),
            *plannedOutput.scheduledPacketization,
            *synchronization.ssrc,
            *synchronization.baseTimestamp,
            *synchronization.clockRate,
            *synchronization.cname,
            senderLead,
            senderReportInterval});
}

::media::Result<MediaSeparateRtpSdpRuntimePlan> scheduledSdp(
    const MediaRealtimeOutputPlanningDraft& output,
    const MediaScheduledRtpOutputPlan& video,
    const MediaScheduledRtpOutputPlan& audio)
{
    const auto& videoRtp = video.transport.remoteRtpEndpoint();
    const auto& audioRtp = audio.transport.remoteRtpEndpoint();
    if (output.sdp.path.empty() || output.sdp.mediaId.empty() ||
        videoRtp.addressFamily() != audioRtp.addressFamily() ||
        videoRtp.numericAddress() != audioRtp.numericAddress() ||
        video.cname.empty() || video.cname != audio.cname) {
        return ::media::Result<MediaSeparateRtpSdpRuntimePlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP SDP requires one complete planner-owned identity"));
    }
    auto identityValidation = MediaSdpSessionIdentity::create(
        output.sdp.mediaId, 0, 0, output.sdp.mediaId,
        videoRtp.addressFamily(), videoRtp.numericAddress(), video.cname);
    if (!identityValidation) {
        return ::media::Result<MediaSeparateRtpSdpRuntimePlan>::failure(
            identityValidation.error());
    }
    return ::media::Result<MediaSeparateRtpSdpRuntimePlan>::success(
        MediaSeparateRtpSdpRuntimePlan{
            output.sdp.path,
            output.sdp.mediaId,
            output.sdp.mediaId,
            videoRtp.addressFamily(),
            videoRtp.numericAddress(),
            video.cname,
            MediaRtpSdpSessionIdPolicy::SharedNtpEpoch,
            MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration});
}

} // namespace

::media::Result<MediaRealtimeAvSyncRuntimePlan>
MediaRealtimeAvSyncRuntimePlanner::plan(
    MediaRealtimeRtpTranscodePlanningDraft& outer,
    MediaRealtimeOutputPlanningDraft& output,
    const MediaRealtimeRtpTranscodeRequest& request,
    MediaAvSyncPlan synchronization,
    MediaRational outputFrameRate)
{
    if (!outer.audioPlan) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "Synchronized runtime planning requires an audio pipeline product"));
    }
    MediaAudioPipelinePlan& audio = *outer.audioPlan;
    if (audio.branchMode != MediaBranchMode::TranscodeFrame &&
        audio.branchMode != MediaBranchMode::CopyPacket) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime planning requires copy or frame transcode audio"));
    }
    if (outer.videoPlan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime planning rejects video packet copy"));
    }
    if (!outer.avSyncComponentBounds) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V runtime requires planner-owned component bounds"));
    }
    auto facts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, audio, *outer.avSyncComponentBounds,
        outer.isolatedAudioInput ? &*outer.isolatedAudioInput : nullptr,
        output, synchronization);
    if (!facts) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            facts.error());
    }
    if (!facts.value().acknowledgementTimeout ||
        !facts.value().terminalDrainWindow ||
        !synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V generation transition timing facts are incomplete"));
    }
    std::optional<MediaAudioCorrectionReachabilityResult> correction;
    if (audio.branchMode == MediaBranchMode::TranscodeFrame) {
        auto selected = MediaAudioCorrectionReachabilityPlanner::plan(
            synchronization, facts.value());
        if (!selected) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                selected.error());
        }
        correction = std::move(selected).value();
        synchronization.audioServo.commandLeadNs = correction->commandLead;
        synchronization.audioServo.compensationWindowNs =
            correction->compensationWindow;
        synchronization.audioServo.frequencyFilterTimeConstantNs =
            correction->frequencyFilterTimeConstant;
        if (auto status = MediaAvSyncPlanValidator::validate(synchronization);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                status.error());
        }
    } else {
        if (auto status =
                MediaAvSyncPlanValidator::validatePolicy(synchronization);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                status.error());
        }
    }
    auto assembly = planAssembly(outer, audio, synchronization, facts.value());
    if (!assembly) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            assembly.error());
    }
    auto edgePolicies = planBoundedEdgePolicies(outer, synchronization);
    if (!edgePolicies) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            edgePolicies.error());
    }

    MediaAvSyncOutputAdapterKind adapter;
    std::optional<MediaRunningTime> activationOutputLead;
    std::optional<std::variant<MediaSeparateRtpOutputRuntimePlan,
                               MediaProjectMpegTsRuntimeOutputPlan>> protocolOutput;
    if (synchronization.rtpOutput) {
        if (synchronization.projectMpegTsOutput ||
            outer.outputLayout != RealtimeOutputStreamLayout::SeparateStreams ||
            outer.outputTransport != MediaOutputTransportKind::RtpAvp ||
            !synchronization.startup.outputLeadNs ||
            !synchronization.rtpOutput->output.senderReportIntervalNs ||
            output.sdp.path.empty() || !audio.resolvedOutput) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP synchronization output facts are incomplete"));
        }
        auto video = scheduledRtpOutput(
            MediaScheduledStream::Video,
            output.videoOutput,
            synchronization.rtpOutput->videoOutput,
            *synchronization.startup.outputLeadNs,
            *synchronization.rtpOutput->output.senderReportIntervalNs);
        auto audio = scheduledRtpOutput(
            MediaScheduledStream::Audio,
            output.audioOutput,
            synchronization.rtpOutput->audioOutput,
            *synchronization.startup.outputLeadNs,
            *synchronization.rtpOutput->output.senderReportIntervalNs);
        if (!video || !audio) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                video ? audio.error() : video.error());
        }
        auto sdp = scheduledSdp(
            output, video.value(), audio.value());
        if (!sdp) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                sdp.error());
        }
        adapter = MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp;
        activationOutputLead = *synchronization.startup.outputLeadNs;
        protocolOutput.emplace(std::in_place_type<MediaSeparateRtpOutputRuntimePlan>,
            MediaSeparateRtpOutputRuntimePlan{
                std::move(video).value(),
                std::move(audio).value(),
                std::move(sdp).value()});
    } else if (synchronization.projectMpegTsOutput) {
        if (synchronization.rtpOutput ||
            outer.outputLayout !=
                RealtimeOutputStreamLayout::MuxedTransportStream ||
            !synchronization.projectMpegTsOutput->outputMux ||
            !facts.value().outputSampleRate || output.muxedOutput.url.empty()) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS synchronization output facts are incomplete"));
        }
        auto accepted = MediaProjectMpegTsOutputPlan::fromAudioVideoEncodedFacts(
            *synchronization.projectMpegTsOutput->outputMux);
        if (!accepted) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                accepted.error());
        }
        if (!outputFrameRate.isKnown() || outputFrameRate.num <= 0 ||
            outputFrameRate.den <= 0 ||
            !facts.value().outputSampleRate ||
            !facts.value().protocolBatchSamples ||
            *facts.value().protocolBatchSamples <= 0) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS emission requires planner-owned output cadences"));
        }
        auto videoCadence = MediaRunningTime::checkedFromTicks(
            1, outputFrameRate.den, outputFrameRate.num);
        auto audioCadence = MediaRunningTime::checkedFromTicks(
            *facts.value().protocolBatchSamples,
            1, *facts.value().outputSampleRate);
        if (!videoCadence || !audioCadence) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                videoCadence ? audioCadence.error() : videoCadence.error());
        }
        auto projectActivationLead =
            accepted.value().muxPlan().transportDecodeLead().checkedAdd(
                accepted.value().muxPlan().startupEmissionPreroll());
        if (!projectActivationLead) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                projectActivationLead.error());
        }
        activationOutputLead = projectActivationLead.value();
        std::optional<std::variant<
            MediaMpegTsUdpOutputPlan,
            MediaMpegTsRtpOutputPlan>> transport;
        if (outer.outputTransport ==
            MediaOutputTransportKind::UdpDatagrams) {
            if (output.muxedOutput.rtpTransport ||
                !output.muxedOutput.sdpPath.empty() ||
                accepted.value().muxPlan().parameters().transportKind !=
                    MediaOutputTransportKind::UdpDatagrams) {
                return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS UDP output rejects RTP transport facts"));
            }
            transport.emplace(
                std::in_place_type<MediaMpegTsUdpOutputPlan>,
                MediaMpegTsUdpOutputPlan{
                    output.muxedOutput.url,
                    MediaOutputResourceKind::ByteSink,
                    MediaMuxSessionKind::ProjectMpegTs});
        } else if (outer.outputTransport ==
                   MediaOutputTransportKind::RtpAvp) {
            if (!output.muxedOutput.rtpTransport ||
                output.muxedOutput.sdpPath.empty() ||
                output.muxedOutput.mediaId.empty() ||
                accepted.value().muxPlan().parameters().transportKind !=
                    MediaOutputTransportKind::RtpAvp) {
                return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS RTP output requires complete planned transport facts"));
            }
            auto pacing = MediaScheduledDatagramPacingPlanner::plan(
                *output.muxedOutput.rtpTransport);
            if (!pacing) {
                return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                    pacing.error());
            }
            auto rtp = MediaMpegTsRtpOutputPlan::create(
                std::move(*output.muxedOutput.rtpTransport),
                output.muxedOutput.sdpPath,
                output.muxedOutput.mediaId,
                MediaRunningTime::fromNanoseconds(NanosecondsPerSecond),
                pacing.value());
            if (!rtp ||
                rtp.value().tsPacketsPerPayload() !=
                    accepted.value().muxPlan().parameters()
                        .maximumPacketsPerDatagram) {
                return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                    rtp ? ::media::ErrorInfo::invalidArgument(
                              "MPEG-TS RTP transport batching differs from the mux plan")
                        : rtp.error());
            }
            transport.emplace(
                std::in_place_type<MediaMpegTsRtpOutputPlan>,
                std::move(rtp).value());
        } else {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::unsupported(
                    "MPEG-TS output transport is unsupported"));
        }
        adapter = MediaAvSyncOutputAdapterKind::ProjectMpegTs;
        outer.videoParameters.globalHeader = true;
        const std::uint64_t maximumQueuedBytes =
            edgePolicies.value().synchronizedPacket.bufferPolicy
                .memoryBudget.maxBytes;
        auto emission = MediaTsDatagramEmissionPlan::create(
            accepted.value().muxPlan(), videoCadence.value(),
            audioCadence.value(), maximumQueuedBytes,
            output.muxedOutput.scheduledWireBytesPerSecond);
        if (!emission) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                emission.error());
        }
        protocolOutput.emplace(std::in_place_type<MediaProjectMpegTsRuntimeOutputPlan>,
            MediaProjectMpegTsRuntimeOutputPlan{
                std::move(accepted).value(),
                MediaMuxSessionKind::ProjectMpegTs,
                std::move(emission).value(),
                outer.outputTransport == MediaOutputTransportKind::RtpAvp
                    ? edgePolicies.value().synchronizedPacket.bufferPolicy
                          .memoryBudget.maxBytes
                    : 0,
                std::move(*transport)});
    } else {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V runtime output authority is unsupported"));
    }

    if (!activationOutputLead) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V runtime output requires planner-owned activation lead"));
    }
    if (!request.deployment) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V runtime output requires deployment transport facts"));
    }
    const MediaAvSyncGroupKey groupKey("realtime.av");
    auto datagramTransport = std::visit(
        [&](const auto& selectedOutput) {
            return MediaRealtimeDatagramTransportPlanner::plan(
                groupKey.value(), *request.deployment, selectedOutput);
        },
        *protocolOutput);
    if (!datagramTransport) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            datagramTransport.error());
    }
    auto transition = MediaAvGenerationTransitionPlanner::plan(
        adapter,
        *synchronization.sourceClockMode,
        audio.branchMode,
        outer.videoPlan.filterActive,
        *facts.value().acknowledgementTimeout,
        *facts.value().terminalDrainWindow);
    return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::success(
        MediaRealtimeAvSyncRuntimePlan{
            std::move(audio),
            std::move(outer.isolatedAudioInput),
            *outer.avSyncComponentBounds,
            groupKey,
            std::move(synchronization),
            std::move(assembly).value(),
            adapter,
            std::move(*protocolOutput),
            std::move(datagramTransport).value(),
            outer.queues,
            std::move(edgePolicies).value(),
            outer.threadingPolicy,
            *activationOutputLead,
            outer.videoPlan.filterActive,
            std::move(transition),
            facts.value(),
            correction
                ? std::optional<MediaAudioCorrectionReachabilityPlan>(
                      correction->correction)
                : std::nullopt});
}

} // namespace media::ffmpeg::graph

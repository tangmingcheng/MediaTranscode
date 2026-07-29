#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.h"

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

::media::Status MediaRealtimeAvSyncRuntimePlanValidator::validate(
    const MediaRealtimeRtpTranscodePlan& outer)
{
    const auto invalid = [](const char* field) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Invalid synchronized runtime product: ") + field));
    };
    if (outer.avSyncRuntime.has_value() ==
        outer.singleStreamOutput.has_value()) {
        return invalid("exactly one output product is required");
    }
    if (!outer.avSyncRuntime) {
        if (outer.audioPlan.enabled) {
            return invalid("required runtime product is absent");
        }
        const auto& output = *outer.singleStreamOutput;
        if (outer.outputLayout == RealtimeOutputStreamLayout::SeparateStreams) {
            if (output.rtpOutput.url.empty() ||
                output.rtpOutput.packetSize <= 0 ||
                output.sdp.path.empty() || !output.mux.expectVideo ||
                output.mux.expectAudio) {
                return invalid("single-stream RTP output");
            }
        } else if (outer.outputLayout ==
                   RealtimeOutputStreamLayout::MuxedTransportStream) {
            if (output.muxedOutput.url.empty() ||
                output.muxedOutput.format.empty() ||
                !output.muxedOutput.muxSessionKind ||
                !output.mux.expectVideo || output.mux.expectAudio) {
                return invalid("single-stream muxed output");
            }
        } else {
            return invalid("single-stream output layout");
        }
        return ::media::Status::success();
    }
    if (outer.audioPlan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime product rejects audio packet copy"));
    }
    if (outer.videoPlan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime product rejects video packet copy"));
    }
    const auto& runtime = *outer.avSyncRuntime;
    if (runtime.groupKey.value() != "realtime.av" ||
        !MediaAvSyncPlanValidator::validate(runtime.synchronization)) {
        return invalid("group or synchronization plan");
    }
    auto selectedBounds = MediaRealtimeAvSyncComponentBoundsPlanner::plan(outer);
    if (!selectedBounds || !outer.avSyncComponentBounds ||
        selectedBounds.value() != *outer.avSyncComponentBounds) {
        return invalid("selected component bounds");
    }
    MediaRealtimeOutputPlanningDraft selectedOutput;
    if (std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(
            runtime.protocolOutput)) {
        const auto& rtp = std::get<MediaSeparateRtpOutputRuntimePlan>(
            runtime.protocolOutput);
        selectedOutput.videoOutput.scheduledPacketization =
            rtp.video.packetization;
        selectedOutput.audioOutput.scheduledPacketization =
            rtp.audio.packetization;
    }
    auto selectedFacts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, selectedOutput, runtime.synchronization);
    if (!selectedFacts || selectedFacts.value() != runtime.planningFacts) {
        return invalid("selected planning facts");
    }
    const auto& assembly = runtime.assembly;
    if (assembly.generationPolicy !=
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange ||
        assembly.initialGeneration != MediaFirstLockedSourceGeneration ||
        assembly.evidencePolicy !=
            MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire ||
        assembly.video.sourceIdentity.empty() ||
        assembly.audio.sourceIdentity.empty() ||
        !runtime.synchronization.startup.videoIdentity ||
        !runtime.synchronization.startup.audioIdentity ||
        assembly.video.sourceIdentity !=
            *runtime.synchronization.startup.videoIdentity ||
        assembly.audio.sourceIdentity !=
            *runtime.synchronization.startup.audioIdentity ||
        runtime.planningFacts.inputVideoIdentity !=
            runtime.synchronization.startup.videoIdentity ||
        runtime.planningFacts.inputAudioIdentity !=
            runtime.synchronization.startup.audioIdentity ||
        assembly.video.decodeOrder !=
            MediaDecodeOrderMode::ReorderedRequiresDecodeTime ||
        assembly.audio.decodeOrder !=
            MediaDecodeOrderMode::PresentationOrderNoReorder ||
        !runtime.synchronization.startup.videoCapacity ||
        !runtime.synchronization.startup.audioCapacity ||
        assembly.video.acquiringCapacity == 0 ||
        assembly.audio.acquiringCapacity == 0 ||
        assembly.video.acquiringCapacity !=
            *runtime.synchronization.startup.videoCapacity ||
        assembly.audio.acquiringCapacity !=
            *runtime.synchronization.startup.audioCapacity ||
        !runtime.synchronization.startup.maximumWaitNs ||
        assembly.video.acquiringTimeout <=
            MediaRunningTime::fromNanoseconds(0) ||
        assembly.audio.acquiringTimeout <=
            MediaRunningTime::fromNanoseconds(0) ||
        assembly.video.acquiringTimeout !=
            *runtime.synchronization.startup.maximumWaitNs ||
        assembly.audio.acquiringTimeout !=
            *runtime.synchronization.startup.maximumWaitNs ||
        !runtime.synchronization.audioServo.minimumUpdateIntervalNs ||
        assembly.startupClockInterval <=
            MediaRunningTime::fromNanoseconds(0) ||
        assembly.startupClockInterval !=
            *runtime.synchronization.audioServo.minimumUpdateIntervalNs) {
        return invalid("production assembly common contract");
    }
    auto expectedCorrection = MediaAudioCorrectionReachabilityPlanner::plan(
        runtime.synchronization, runtime.planningFacts);
    if (!expectedCorrection ||
        runtime.audioCorrection != expectedCorrection.value().correction ||
        !runtime.synchronization.audioServo.commandLeadNs ||
        !runtime.synchronization.audioServo.compensationWindowNs ||
        !runtime.synchronization.audioServo.frequencyFilterTimeConstantNs ||
        *runtime.synchronization.audioServo.commandLeadNs !=
            expectedCorrection.value().commandLead ||
        *runtime.synchronization.audioServo.compensationWindowNs !=
            expectedCorrection.value().compensationWindow ||
        *runtime.synchronization.audioServo.frequencyFilterTimeConstantNs !=
            expectedCorrection.value().frequencyFilterTimeConstant) {
        return invalid("audio correction derivation");
    }
    if (runtime.queues.metadata != outer.queues.metadata ||
        runtime.queues.packet != outer.queues.packet ||
        runtime.queues.frame != outer.queues.frame ||
        runtime.queues.mux != outer.queues.mux) {
        return invalid("queue product");
    }
    if (runtime.edgePolicies !=
        MediaRealtimeEdgePolicyPlanner::plan(runtime.queues)) {
        return invalid("edge-policy product");
    }
    if (runtime.threadingPolicy.mode != MediaThreadingMode::PerNodeWorker ||
        runtime.threadingPolicy.priority != MediaThreadPriority::High ||
        runtime.threadingPolicy.maxWorkerThreads != 0 ||
        runtime.threadingPolicy.pinWorkers ||
        !runtime.threadingPolicy.collectWorkerMetrics) {
        return invalid("threading product");
    }
    if (runtime.transition.acknowledgementTimeout <=
            MediaRunningTime::fromNanoseconds(0) ||
        runtime.transition.terminalDrainWindow <=
            MediaRunningTime::fromNanoseconds(0) ||
        !runtime.planningFacts.acknowledgementTimeout ||
        !runtime.planningFacts.terminalDrainWindow ||
        runtime.transition.acknowledgementTimeout !=
            *runtime.planningFacts.acknowledgementTimeout ||
        runtime.transition.terminalDrainWindow !=
            *runtime.planningFacts.terminalDrainWindow) {
        return invalid("transition timeout");
    }
    if (runtime.outputAdapter !=
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp &&
        runtime.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        return invalid("output adapter");
    }
    const auto expected = MediaAvGenerationTransitionPlanner::plan(
        runtime.outputAdapter,
        runtime.transition.acknowledgementTimeout,
        runtime.transition.terminalDrainWindow);
    if (runtime.transition.participants.size() !=
        expected.participants.size()) {
        return invalid("transition participant count");
    }
    for (std::size_t index = 0; index < expected.participants.size(); ++index) {
        if (runtime.transition.participants[index].participant !=
                expected.participants[index].participant ||
            runtime.transition.participants[index].requiredChildren !=
                expected.participants[index].requiredChildren) {
            return invalid("transition participant children");
        }
    }
    const auto& correction = runtime.audioCorrection;
    const bool reachabilitySumRepresentable =
        correction.worstCaseInFlightSamples >= 0 &&
        correction.mailboxDeliveryMarginSamples > 0 &&
        correction.maximumResamplerOutputBlockSamples > 0 &&
        correction.worstCaseInFlightSamples <=
            std::numeric_limits<std::int64_t>::max() -
                correction.mailboxDeliveryMarginSamples &&
        correction.worstCaseInFlightSamples +
                correction.mailboxDeliveryMarginSamples <=
            std::numeric_limits<std::int64_t>::max() -
                correction.maximumResamplerOutputBlockSamples;
    if (correction.outputSampleRate <= 0 ||
        correction.epochOutputSampleIndex != 0 ||
        correction.worstCaseInFlightSamples < 0 ||
        correction.protocolBatchSamples <= 0 ||
        correction.mailboxDeliveryMarginSamples <= 0 ||
        correction.maximumResamplerOutputBlockSamples <= 0 ||
        correction.mailboxCapacity != runtime.queues.metadata ||
        !reachabilitySumRepresentable ||
        correction.commandLeadSamples <=
            correction.worstCaseInFlightSamples +
                correction.mailboxDeliveryMarginSamples +
                correction.maximumResamplerOutputBlockSamples ||
        !runtime.synchronization.audioServo.outputSampleRate ||
        correction.outputSampleRate !=
            *runtime.synchronization.audioServo.outputSampleRate) {
        return invalid("audio correction reachability");
    }
    if (!runtime.synchronization.sourceClockMode) {
        return invalid("source clock mode");
    }
    if (*runtime.synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (!runtime.synchronization.rtpInput ||
            outer.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
            !outer.input.rtpTransport || !outer.audioInput.rtpTransport ||
            !std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
                assembly.inputClock) ||
            !std::holds_alternative<MediaRtpTimestampDeltaDurationPlan>(
                assembly.video.duration) ||
            !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
                assembly.audio.duration)) {
            return invalid("RTP input clock assembly");
        }
        const auto& input = *runtime.synchronization.rtpInput;
        const auto& videoDuration =
            std::get<MediaRtpTimestampDeltaDurationPlan>(
                assembly.video.duration);
        const auto& audioDuration =
            std::get<MediaPlannedAudioSamplesDurationPlan>(
                assembly.audio.duration);
        if (!input.videoInput.clockRate || !input.audioInput.clockRate ||
            videoDuration.clockRate != *input.videoInput.clockRate ||
            audioDuration.sampleRate != *input.audioInput.clockRate ||
            !runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
            audioDuration.samplesPerAccessUnit !=
                *runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
            std::get<MediaRtpInputClockAssemblyPlan>(assembly.inputClock)
                    .commonEpochPolicy != input.input.commonEpochPolicy) {
            return invalid("RTP input duration facts");
        }
    } else if (*runtime.synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        if (!runtime.synchronization.mpegTsInput ||
            outer.inputType != RealtimeInputType::MpegTsUdp ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::MuxedTransportStream ||
            !std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
                assembly.inputClock) ||
            !std::holds_alternative<MediaPacketDurationPlan>(
                assembly.video.duration) ||
            !std::get<MediaPacketDurationPlan>(assembly.video.duration)
                 .requirePositiveDuration ||
            !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
                assembly.audio.duration) ||
            !outer.input.mpegTs ||
            !runtime.planningFacts.inputVideoPacketDuration ||
            !runtime.planningFacts.inputAudioPacketDuration ||
            runtime.planningFacts.inputVideoPacketDuration !=
                outer.input.mpegTs->videoPacketDuration ||
            runtime.planningFacts.inputAudioPacketDuration !=
                outer.input.mpegTs->audioPacketDuration) {
            return invalid("MPEG-TS input clock assembly");
        }
    } else if (*runtime.synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (!runtime.synchronization.demuxTimestampInput ||
            outer.inputType != RealtimeInputType::Url ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::SessionDescribed ||
            !std::holds_alternative<
                MediaDemuxTimestampInputClockAssemblyPlan>(
                    assembly.inputClock) ||
            !std::holds_alternative<MediaPacketDurationPlan>(
                assembly.video.duration) ||
            !std::holds_alternative<MediaPacketDurationPlan>(
                assembly.audio.duration) ||
            !std::get<MediaPacketDurationPlan>(assembly.video.duration)
                 .requirePositiveDuration ||
            !std::get<MediaPacketDurationPlan>(assembly.audio.duration)
                 .requirePositiveDuration) {
            return invalid("demux timestamp input clock assembly");
        }
        const auto& input =
            *runtime.synchronization.demuxTimestampInput;
        const auto& selected =
            std::get<MediaDemuxTimestampInputClockAssemblyPlan>(
                assembly.inputClock);
        if (!input.firstWindowMaximumSkewNs ||
            !input.timestampRegressionLimitNs ||
            !input.discontinuityThresholdNs || !input.initialGeneration ||
            selected.videoTimeBase.num != input.videoTimeBase.num ||
            selected.videoTimeBase.den != input.videoTimeBase.den ||
            selected.audioTimeBase.num != input.audioTimeBase.num ||
            selected.audioTimeBase.den != input.audioTimeBase.den ||
            selected.firstWindowMaximumSkew !=
                *input.firstWindowMaximumSkewNs ||
            selected.timestampRegressionLimit !=
                *input.timestampRegressionLimitNs ||
            selected.discontinuityThreshold !=
                *input.discontinuityThresholdNs ||
            selected.initialGeneration != *input.initialGeneration) {
            return invalid("demux timestamp input policy");
        }
    } else {
        return invalid("source clock mode");
    }

    if (runtime.synchronization.rtpOutput) {
        if (runtime.synchronization.projectMpegTsOutput ||
            outer.outputLayout != RealtimeOutputStreamLayout::SeparateStreams ||
            outer.outputTransport != MediaOutputTransportKind::RtpAvp ||
            runtime.outputAdapter !=
                MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp ||
            !std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(
                runtime.protocolOutput)) {
            return invalid("separate RTP output authority");
        }
        const auto& selected = *runtime.synchronization.rtpOutput;
        const auto& output =
            std::get<MediaSeparateRtpOutputRuntimePlan>(
                runtime.protocolOutput);
        if (!selected.videoOutput.ssrc ||
            !selected.videoOutput.baseTimestamp ||
            !selected.videoOutput.clockRate ||
            !selected.audioOutput.ssrc ||
            !selected.audioOutput.baseTimestamp ||
            !selected.audioOutput.clockRate ||
            !selected.output.senderReportIntervalNs ||
            output.sdp.path.empty() ||
            output.video.ssrc != *selected.videoOutput.ssrc ||
            output.video.baseTimestamp !=
                *selected.videoOutput.baseTimestamp ||
            output.video.clockRate != *selected.videoOutput.clockRate ||
            output.audio.ssrc != *selected.audioOutput.ssrc ||
            output.audio.baseTimestamp !=
                *selected.audioOutput.baseTimestamp ||
            output.audio.clockRate != *selected.audioOutput.clockRate ||
            output.video.senderReportInterval !=
                *selected.output.senderReportIntervalNs ||
            output.audio.senderReportInterval !=
                *selected.output.senderReportIntervalNs) {
            return invalid("separate RTP protocol output");
        }
    } else if (runtime.synchronization.projectMpegTsOutput) {
        if (runtime.synchronization.rtpOutput ||
            outer.outputLayout !=
                RealtimeOutputStreamLayout::MuxedTransportStream ||
            outer.outputTransport != MediaOutputTransportKind::UdpDatagrams ||
            runtime.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs ||
            !std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(
                runtime.protocolOutput) ||
            !runtime.synchronization.projectMpegTsOutput->outputMux) {
            return invalid("Project MPEG-TS output authority");
        }
        const auto& output =
            std::get<MediaProjectMpegTsRuntimeOutputPlan>(
                runtime.protocolOutput);
        if (output.url.empty() ||
            output.resourceKind != MediaOutputResourceKind::ByteSink ||
            output.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
            output.protocol.audioSampleRate() != correction.outputSampleRate ||
            output.protocol.muxPlan().parameters() !=
                runtime.synchronization.projectMpegTsOutput->outputMux
                    ->parameters()) {
            return invalid("Project MPEG-TS protocol output");
        }
    } else {
        return invalid("output authority");
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

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
    if (!outer.avSyncRuntime) {
        if (outer.audioPlan.enabled) return invalid("required runtime product is absent");
        return ::media::Status::success();
    }
    if (outer.audioPlan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime product rejects audio packet copy"));
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
    auto selectedFacts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, runtime.synchronization);
    if (!selectedFacts || selectedFacts.value() != runtime.planningFacts) {
        return invalid("selected planning facts");
    }
    const auto& assembly = runtime.assembly;
    if (assembly.generationPolicy !=
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange ||
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
    if (outer.videoOutput.writePacingEnabled ||
        outer.videoOutput.writePacingBytesPerSecond != 0 ||
        outer.videoOutput.writePacingBurstBytes != 0 ||
        outer.audioOutput.writePacingEnabled ||
        outer.audioOutput.writePacingBytesPerSecond != 0 ||
        outer.audioOutput.writePacingBurstBytes != 0 ||
        outer.videoMux.pacingPolicy.enablePacing ||
        outer.videoMux.startupDelayMs != 0 ||
        outer.audioMux.pacingPolicy.enablePacing ||
        outer.audioMux.startupDelayMs != 0 ||
        outer.avStartBarrier.expectVideo || outer.avStartBarrier.expectAudio ||
        outer.avStartBarrier.requireVideoKeyFrame) {
        return invalid("legacy pacing or barrier authority");
    }

    if (*runtime.synchronization.topology ==
        MediaAvSyncTopology::SeparateRtpToSeparateRtp) {
        if (outer.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
            outer.outputLayout != RealtimeOutputStreamLayout::SeparateStreams ||
            runtime.outputAdapter !=
                MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp ||
            !std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(
                runtime.protocolOutput) || !runtime.synchronization.rtp ||
            !std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
                assembly.inputClock) ||
            std::get<MediaRtpInputClockAssemblyPlan>(assembly.inputClock)
                    .commonEpochPolicy !=
                MediaRtpCommonEpochPolicy::
                    EarliestLockedSenderReportSourceTime ||
            !std::holds_alternative<MediaRtpTimestampDeltaDurationPlan>(
                assembly.video.duration) ||
            !std::holds_alternative<MediaPlannedAudioSamplesDurationPlan>(
                assembly.audio.duration)) {
            return invalid("RTP topology and adapter");
        }
        const auto& videoDuration =
            std::get<MediaRtpTimestampDeltaDurationPlan>(
                assembly.video.duration);
        const auto& audioDuration =
            std::get<MediaPlannedAudioSamplesDurationPlan>(
                assembly.audio.duration);
        if (!runtime.planningFacts.inputVideoClockRate ||
            videoDuration.clockRate <= 0 ||
            videoDuration.clockRate !=
                *runtime.planningFacts.inputVideoClockRate ||
            !runtime.synchronization.rtp->videoInput.clockRate ||
            videoDuration.clockRate !=
                *runtime.synchronization.rtp->videoInput.clockRate ||
            videoDuration.terminalPolicy !=
                MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta ||
            !runtime.planningFacts.inputAudioSampleRate ||
            audioDuration.sampleRate <= 0 ||
            audioDuration.sampleRate !=
                *runtime.planningFacts.inputAudioSampleRate ||
            !runtime.synchronization.rtp->audioInput.clockRate ||
            audioDuration.sampleRate !=
                *runtime.synchronization.rtp->audioInput.clockRate ||
            !runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
            audioDuration.samplesPerAccessUnit == 0 ||
            audioDuration.samplesPerAccessUnit !=
                *runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
            (outer.videoPlan.inputCodecName != "h264" &&
             outer.videoPlan.inputCodecName != "hevc")) {
            return invalid("RTP production assembly duration");
        }
        const auto& output =
            std::get<MediaSeparateRtpOutputRuntimePlan>(runtime.protocolOutput);
        auto sdpIdentity = MediaSdpSessionIdentity::create(
            output.sdp.originUsername, 0, 0, output.sdp.sessionName,
            output.sdp.originAddressFamily,
            output.sdp.originNumericAddress, output.sdp.cname);
        const auto samePacketization = [](const auto& left, const auto& right) {
            return left.streamKind() == right.streamKind() &&
                left.codecName() == right.codecName() &&
                left.streamTimeBaseNumerator() == right.streamTimeBaseNumerator() &&
                left.streamTimeBaseDenominator() == right.streamTimeBaseDenominator() &&
                left.packetizationMode() == right.packetizationMode() &&
                left.payloadType() == right.payloadType() &&
                left.maximumDatagramBytes() == right.maximumDatagramBytes() &&
                left.maximumAccessUnitSamples() == right.maximumAccessUnitSamples();
        };
        const auto validRtp = [&](const MediaScheduledRtpOutputPlan& candidate,
                                  const MediaAvSyncRtpOutputStreamPlan& sync,
                                  const MediaScheduledRtpPacketizationPlan& selected,
                                  const std::string& expectedCodec,
                                  MediaScheduledStream stream,
                                  MediaScheduledRtpPacketizationMode mode) {
            const auto family = candidate.transport.addressFamily();
            const bool video = stream == MediaScheduledStream::Video;
            return sync.payloadType && sync.clockRate && sync.ssrc &&
                sync.baseTimestamp && sync.cname &&
                candidate.stream == stream &&
                samePacketization(candidate.packetization, selected) &&
                candidate.packetization.streamKind() ==
                    (video
                         ? MediaStreamKind::Video
                         : MediaStreamKind::Audio) &&
                candidate.packetization.codecName() == expectedCodec &&
                candidate.packetization.streamTimeBaseNumerator() == 1 &&
                candidate.packetization.streamTimeBaseDenominator() ==
                    *sync.clockRate &&
                candidate.packetization.packetizationMode() == mode &&
                candidate.packetization.payloadType() == *sync.payloadType &&
                candidate.packetization.maximumDatagramBytes() > 0 &&
                (video
                    ? !candidate.packetization.maximumAccessUnitSamples()
                    : candidate.packetization.maximumAccessUnitSamples() ==
                          correction.protocolBatchSamples) &&
                candidate.transport.maximumDatagramBytes() ==
                    candidate.packetization.maximumDatagramBytes() &&
                candidate.transport.remoteRtpEndpoint().addressFamily() == family &&
                candidate.transport.remoteRtcpEndpoint().addressFamily() == family &&
                candidate.transport.localNumericAddress() ==
                    (family == MediaIpAddressFamily::Ipv4 ? "0.0.0.0" : "::") &&
                !candidate.transport.remoteRtpEndpoint().numericAddress().empty() &&
                candidate.transport.remoteRtcpEndpoint().numericAddress() ==
                    candidate.transport.remoteRtpEndpoint().numericAddress() &&
                candidate.transport.remoteRtpEndpoint().port() > 0 &&
                candidate.transport.remoteRtcpEndpoint().port() ==
                    candidate.transport.remoteRtpEndpoint().port() + 1 &&
                candidate.transport.localPortPolicy().kind() ==
                    MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent &&
                !candidate.transport.localPortPolicy().rtpPort() &&
                !candidate.transport.localPortPolicy().rtcpPort() &&
                candidate.transport.maximumDatagramBytes() <=
                    static_cast<std::size_t>(std::numeric_limits<int>::max() / 2) &&
                candidate.transport.sendBufferBytes() ==
                    static_cast<int>(candidate.transport.maximumDatagramBytes() * 2) &&
                candidate.transport.ioBehavior() ==
                    MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure &&
                candidate.ssrc == *sync.ssrc &&
                candidate.baseTimestamp == *sync.baseTimestamp &&
                candidate.clockRate == *sync.clockRate &&
                candidate.cname == *sync.cname &&
                runtime.synchronization.startup.outputLeadNs &&
                runtime.synchronization.rtp->output.senderReportIntervalNs &&
                candidate.senderLead ==
                    *runtime.synchronization.startup.outputLeadNs &&
                candidate.senderReportInterval ==
                    *runtime.synchronization.rtp->output.senderReportIntervalNs;
        };
        if (!sdpIdentity || output.sdp.path.empty() ||
            output.sdp.path != outer.sdp.path ||
            output.sdp.originUsername != outer.sdp.mediaId ||
            output.sdp.sessionName != outer.sdp.mediaId ||
            output.sdp.sessionIdPolicy !=
                MediaRtpSdpSessionIdPolicy::SharedNtpEpoch ||
            output.sdp.sessionVersionPolicy !=
                MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration ||
            !outer.videoOutput.scheduledPacketization ||
            !outer.audioOutput.scheduledPacketization ||
            outer.videoPlan.outputCodecName.empty() ||
            !outer.audioPlan.resolvedOutput ||
            !validRtp(output.video,
                      runtime.synchronization.rtp->videoOutput,
                      *outer.videoOutput.scheduledPacketization,
                      outer.videoPlan.outputCodecName,
                      MediaScheduledStream::Video,
                      MediaScheduledRtpPacketizationMode::H264AnnexB) ||
            !validRtp(output.audio,
                      runtime.synchronization.rtp->audioOutput,
                      *outer.audioOutput.scheduledPacketization,
                      outer.audioPlan.resolvedOutput->codecName(),
                      MediaScheduledStream::Audio,
                      MediaScheduledRtpPacketizationMode::AacLatm) ||
            output.video.transport.addressFamily() !=
                output.audio.transport.addressFamily() ||
            output.sdp.originAddressFamily !=
                output.video.transport.addressFamily() ||
            output.sdp.originNumericAddress !=
                output.video.transport.remoteRtpEndpoint().numericAddress() ||
            output.sdp.originNumericAddress !=
                output.audio.transport.remoteRtpEndpoint().numericAddress() ||
            output.sdp.cname != output.video.cname ||
            output.sdp.cname != output.audio.cname ||
            output.video.transport.remoteRtpEndpoint().numericAddress() !=
                output.audio.transport.remoteRtpEndpoint().numericAddress() ||
            output.audio.transport.remoteRtpEndpoint().port() !=
                output.video.transport.remoteRtpEndpoint().port() + 2) {
            return invalid("RTP protocol output");
        }
    } else if (*runtime.synchronization.topology ==
               MediaAvSyncTopology::MpegTsToMpegTs) {
        if (outer.inputType != RealtimeInputType::MpegTsUdp ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::MuxedTransportStream ||
            outer.outputLayout !=
                RealtimeOutputStreamLayout::MuxedTransportStream ||
            runtime.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs ||
            !std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(
                runtime.protocolOutput) ||
            !std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
                assembly.inputClock) ||
            !std::holds_alternative<MediaPacketDurationPlan>(
                assembly.video.duration) ||
            !std::holds_alternative<MediaPacketDurationPlan>(
                assembly.audio.duration) ||
            !std::get<MediaPacketDurationPlan>(assembly.video.duration)
                 .requirePositiveDuration ||
            !std::get<MediaPacketDurationPlan>(assembly.audio.duration)
                 .requirePositiveDuration ||
            runtime.planningFacts.inputVideoClockRate ||
            runtime.planningFacts.inputAudioSamplesPerAccessUnit ||
            !runtime.planningFacts.inputAudioSampleRate ||
            *runtime.planningFacts.inputAudioSampleRate <= 0 ||
            !outer.input.mpegTs ||
            outer.input.mpegTs->initialSourceGeneration !=
                MediaFirstLockedSourceGeneration ||
            !outer.input.mpegTs->videoPacketDuration ||
            !outer.input.mpegTs->audioPacketDuration ||
            !runtime.planningFacts.inputVideoPacketDuration ||
            !runtime.planningFacts.inputAudioPacketDuration ||
            runtime.planningFacts.inputVideoPacketDuration !=
                outer.input.mpegTs->videoPacketDuration ||
            runtime.planningFacts.inputAudioPacketDuration !=
                outer.input.mpegTs->audioPacketDuration ||
            runtime.planningFacts.inputVideoPacketDuration->packetDuration <= 0 ||
            runtime.planningFacts.inputAudioPacketDuration->packetDuration <= 0 ||
            runtime.planningFacts.inputVideoPacketDuration->timeBase.num <= 0 ||
            runtime.planningFacts.inputVideoPacketDuration->timeBase.den <= 0 ||
            runtime.planningFacts.inputAudioPacketDuration->timeBase.num <= 0 ||
            runtime.planningFacts.inputAudioPacketDuration->timeBase.den <= 0) {
            return invalid("MPEG-TS topology and adapter");
        }
        const auto& output = std::get<MediaProjectMpegTsRuntimeOutputPlan>(
            runtime.protocolOutput);
        if (output.url.empty() || output.url != outer.muxedOutput.url ||
            output.resourceKind != MediaOutputResourceKind::ByteSink ||
            outer.muxedOutput.outputResourceKind != output.resourceKind ||
            output.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
            outer.muxedOutput.muxSessionKind != output.muxSessionKind ||
            output.protocol.audioSampleRate() != correction.outputSampleRate ||
            !runtime.synchronization.ts ||
            !runtime.synchronization.ts->outputMux ||
            output.protocol.muxPlan().parameters() !=
                runtime.synchronization.ts->outputMux->parameters()) {
            return invalid("MPEG-TS protocol output");
        }
    } else {
        return invalid("synchronization topology");
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

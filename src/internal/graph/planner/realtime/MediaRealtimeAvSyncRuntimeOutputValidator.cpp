#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeOutputValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <limits>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalidOutput(const char* field)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            std::string("Invalid synchronized output product: ") + field));
}

bool samePacketization(
    const MediaScheduledRtpPacketizationPlan& left,
    const MediaScheduledRtpPacketizationPlan& right)
{
    return left.streamKind() == right.streamKind() &&
        left.codecName() == right.codecName() &&
        left.streamTimeBaseNumerator() ==
            right.streamTimeBaseNumerator() &&
        left.streamTimeBaseDenominator() ==
            right.streamTimeBaseDenominator() &&
        left.packetizationMode() == right.packetizationMode() &&
        left.payloadType() == right.payloadType() &&
        left.maximumDatagramBytes() == right.maximumDatagramBytes() &&
        left.maximumAccessUnitSamples() ==
            right.maximumAccessUnitSamples();
}

bool validRtpStream(
    const MediaScheduledRtpOutputPlan& candidate,
    const MediaAvSyncRtpOutputStreamPlan& synchronization,
    const MediaScheduledRtpPacketizationPlan& selected,
    const std::string& expectedCodec,
    MediaScheduledStream stream,
    MediaScheduledRtpPacketizationMode packetizationMode,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    const auto family = candidate.transport.addressFamily();
    const bool video = stream == MediaScheduledStream::Video;
    return synchronization.payloadType &&
        synchronization.clockRate &&
        synchronization.ssrc &&
        synchronization.baseTimestamp &&
        synchronization.cname &&
        candidate.stream == stream &&
        samePacketization(candidate.packetization, selected) &&
        candidate.packetization.streamKind() ==
            (video ? MediaStreamKind::Video : MediaStreamKind::Audio) &&
        candidate.packetization.codecName() == expectedCodec &&
        candidate.packetization.streamTimeBaseNumerator() == 1 &&
        candidate.packetization.streamTimeBaseDenominator() ==
            *synchronization.clockRate &&
        candidate.packetization.packetizationMode() == packetizationMode &&
        candidate.packetization.payloadType() ==
            *synchronization.payloadType &&
        candidate.packetization.maximumDatagramBytes() > 0 &&
        (video
             ? !candidate.packetization.maximumAccessUnitSamples()
             : runtime.planningFacts.protocolBatchSamples &&
                   candidate.packetization.maximumAccessUnitSamples() ==
                       runtime.planningFacts.protocolBatchSamples) &&
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
            static_cast<std::size_t>(
                std::numeric_limits<int>::max() / 2) &&
        candidate.transport.sendBufferBytes() ==
            static_cast<int>(
                candidate.transport.maximumDatagramBytes() * 2) &&
        candidate.transport.ioBehavior() ==
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure &&
        candidate.ssrc == *synchronization.ssrc &&
        candidate.baseTimestamp == *synchronization.baseTimestamp &&
        candidate.clockRate == *synchronization.clockRate &&
        candidate.cname == *synchronization.cname &&
        runtime.synchronization.startup.outputLeadNs &&
        runtime.synchronization.rtpOutput &&
        runtime.synchronization.rtpOutput->output.senderReportIntervalNs &&
        candidate.senderLead ==
            *runtime.synchronization.startup.outputLeadNs &&
        candidate.senderReportInterval ==
            *runtime.synchronization.rtpOutput->output.senderReportIntervalNs;
}

::media::Status validateSeparateRtpOutput(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    if (runtime.synchronization.projectMpegTsOutput ||
        !runtime.synchronization.rtpOutput ||
        outer.outputLayout != RealtimeOutputStreamLayout::SeparateStreams ||
        outer.outputTransport != MediaOutputTransportKind::RtpAvp ||
        runtime.outputAdapter !=
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp ||
        !std::holds_alternative<MediaSeparateRtpOutputRuntimePlan>(
            runtime.protocolOutput)) {
        return invalidOutput("separate RTP authority");
    }

    const auto& synchronization = *runtime.synchronization.rtpOutput;
    const auto& output =
        std::get<MediaSeparateRtpOutputRuntimePlan>(
            runtime.protocolOutput);
    if (!runtime.planningFacts.outputVideoRtpPacketization ||
        !runtime.planningFacts.outputAudioRtpPacketization) {
        return invalidOutput("planner-owned RTP packetization facts");
    }
    auto sdpIdentity = MediaSdpSessionIdentity::create(
        output.sdp.originUsername, 0, 0, output.sdp.sessionName,
        output.sdp.originAddressFamily,
        output.sdp.originNumericAddress, output.sdp.cname);
    if (!sdpIdentity || output.sdp.path.empty() ||
        output.sdp.originUsername.empty() ||
        output.sdp.sessionName.empty() ||
        output.sdp.originUsername != output.sdp.sessionName ||
        output.sdp.sessionIdPolicy !=
            MediaRtpSdpSessionIdPolicy::SharedNtpEpoch ||
        output.sdp.sessionVersionPolicy !=
            MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration ||
        outer.videoPlan.outputCodecName.empty() ||
        !runtime.audioPipeline.resolvedOutput ||
        !validRtpStream(
            output.video, synchronization.videoOutput,
            *runtime.planningFacts.outputVideoRtpPacketization,
            outer.videoPlan.outputCodecName,
            MediaScheduledStream::Video,
            runtime.planningFacts.outputVideoRtpPacketization
                ->packetizationMode(),
            runtime) ||
        !validRtpStream(
            output.audio, synchronization.audioOutput,
            *runtime.planningFacts.outputAudioRtpPacketization,
            runtime.audioPipeline.resolvedOutput->codecName(),
            MediaScheduledStream::Audio,
            MediaScheduledRtpPacketizationMode::AacLatm,
            runtime) ||
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
        return invalidOutput("separate RTP protocol facts");
    }
    return ::media::Status::success();
}

::media::Status validateProjectMpegTsOutput(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    if (runtime.synchronization.rtpOutput ||
        !runtime.synchronization.projectMpegTsOutput ||
        !runtime.synchronization.projectMpegTsOutput
             ->useSharedNtpEpoch ||
        outer.outputLayout !=
            RealtimeOutputStreamLayout::MuxedTransportStream ||
        runtime.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs ||
        runtime.planningFacts.outputVideoRtpPacketization ||
        runtime.planningFacts.outputAudioRtpPacketization ||
        !outer.videoParameters.globalHeader ||
        !*outer.videoParameters.globalHeader ||
        !std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(
            runtime.protocolOutput) ||
        !runtime.synchronization.projectMpegTsOutput->outputMux) {
        return invalidOutput("Project MPEG-TS authority");
    }
    const auto& output =
        std::get<MediaProjectMpegTsRuntimeOutputPlan>(
            runtime.protocolOutput);
    const auto& mux = output.protocol.muxPlan().parameters();
    auto expectedEmission = MediaTsDatagramEmissionPlan::create(
        output.protocol.muxPlan(),
        output.emission.videoInitialServiceWindow(),
        output.emission.audioInitialServiceWindow());
    const auto* program = output.protocol.muxPlan().audioVideoProgram();
    const bool sampleRateMatches = program &&
        program->aac.samplingFrequencyIndex < MediaAacSampleRates.size() &&
        runtime.planningFacts.outputSampleRate &&
        MediaAacSampleRates[program->aac.samplingFrequencyIndex] ==
            *runtime.planningFacts.outputSampleRate;
    if (output.muxSessionKind !=
            MediaMuxSessionKind::ProjectMpegTs ||
        !sampleRateMatches || !expectedEmission ||
        output.emission != expectedEmission.value() ||
        mux !=
            runtime.synchronization.projectMpegTsOutput->outputMux
                ->parameters()) {
        return invalidOutput("Project MPEG-TS protocol facts");
    }
    if (outer.outputTransport == MediaOutputTransportKind::UdpDatagrams) {
        const auto* udp =
            std::get_if<MediaMpegTsUdpOutputPlan>(&output.transport);
        if (!udp || mux.transportKind !=
                         MediaOutputTransportKind::UdpDatagrams ||
            output.emission.perDatagramOverheadBytes() != 0 ||
            *runtime.synchronization.projectMpegTsOutput
                 ->useSharedNtpEpoch ||
            udp->url.empty() ||
            udp->resourceKind != MediaOutputResourceKind::ByteSink ||
            udp->muxSessionKind != MediaMuxSessionKind::ProjectMpegTs) {
            return invalidOutput("Project MPEG-TS UDP transport facts");
        }
        return ::media::Status::success();
    }
    if (outer.outputTransport != MediaOutputTransportKind::RtpAvp) {
        return invalidOutput("Project MPEG-TS transport kind");
    }
    const auto* rtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(&output.transport);
    if (!rtp || mux.transportKind != MediaOutputTransportKind::RtpAvp ||
        !*runtime.synchronization.projectMpegTsOutput
              ->useSharedNtpEpoch) {
        return invalidOutput("Project MPEG-TS RTP transport variant");
    }
    const auto& sender = rtp->transport();
    const auto& remoteRtp = sender.remoteRtpEndpoint();
    const auto& remoteRtcp = sender.remoteRtcpEndpoint();
    auto maximumPackets = MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
        rtp->maximumDatagramBytes());
    auto sdpIdentity = MediaSdpSessionIdentity::create(
        rtp->sdp().originUsername, 0, 0, rtp->sdp().sessionName,
        rtp->sdp().originAddressFamily,
        rtp->sdp().originNumericAddress, rtp->sdp().cname);
    if (!maximumPackets || !sdpIdentity ||
        output.emission.perDatagramOverheadBytes() != 12 ||
        output.emission.maximumWireDatagramBytes() >
            rtp->maximumDatagramBytes() ||
        rtp->payloadType() != 33 || rtp->clockRate() != 90'000 ||
        rtp->ssrc() == 0 || rtp->cname().empty() ||
        rtp->initialSequenceNumber() !=
            MediaRtpOutputIdentityPlanner::stableSequenceNumber(
                rtp->sdp().originUsername + ".output.mp2t.sequence") ||
        rtp->senderReportInterval() <=
            MediaRunningTime::fromNanoseconds(0) ||
        !runtime.synchronization.recovery.reacquisitionTimeoutNs ||
        rtp->senderReportInterval() >=
            *runtime.synchronization.recovery.reacquisitionTimeoutNs ||
        rtp->maximumDatagramBytes() != sender.maximumDatagramBytes() ||
        rtp->maximumDatagramBytes() >
            static_cast<std::size_t>(
                (std::numeric_limits<int>::max)()) ||
        rtp->tsPacketsPerPayload() != maximumPackets.value() ||
        mux.maximumPacketsPerDatagram != rtp->tsPacketsPerPayload() ||
        sender.ioBehavior() !=
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure ||
        sender.localPortPolicy().kind() !=
            MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent ||
        sender.localPortPolicy().rtpPort() ||
        sender.localPortPolicy().rtcpPort() ||
        sender.localNumericAddress() !=
            (remoteRtp.addressFamily() == MediaIpAddressFamily::Ipv4
                 ? "0.0.0.0"
                 : "::") ||
        remoteRtp.port() == 0 || (remoteRtp.port() % 2) != 0 ||
        remoteRtcp.port() != remoteRtp.port() + 1 ||
        remoteRtcp.addressFamily() != remoteRtp.addressFamily() ||
        remoteRtcp.numericAddress() != remoteRtp.numericAddress() ||
        rtp->sdp().path.empty() ||
        rtp->sdp().originUsername.empty() ||
        rtp->sdp().originUsername != rtp->sdp().sessionName ||
        rtp->sdp().originAddressFamily != remoteRtp.addressFamily() ||
        rtp->sdp().originNumericAddress != remoteRtp.numericAddress() ||
        rtp->sdp().cname != rtp->cname()) {
        return invalidOutput("Project MPEG-TS RTP protocol facts");
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRealtimeAvSyncRuntimeOutputValidator::validate(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    switch (runtime.outputAdapter) {
    case MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp:
        return validateSeparateRtpOutput(outer, runtime);
    case MediaAvSyncOutputAdapterKind::ProjectMpegTs:
        return validateProjectMpegTsOutput(outer, runtime);
    }
    return invalidOutput("unsupported output adapter");
}

} // namespace media::ffmpeg::graph

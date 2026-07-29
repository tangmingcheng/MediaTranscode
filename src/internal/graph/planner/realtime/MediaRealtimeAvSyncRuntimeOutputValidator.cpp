#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeOutputValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
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
             : candidate.packetization.maximumAccessUnitSamples() ==
                   runtime.audioCorrection.protocolBatchSamples) &&
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
        !outer.audioPlan.resolvedOutput ||
        !validRtpStream(
            output.video, synchronization.videoOutput,
            *runtime.planningFacts.outputVideoRtpPacketization,
            outer.videoPlan.outputCodecName,
            MediaScheduledStream::Video,
            MediaScheduledRtpPacketizationMode::H264AnnexB,
            runtime) ||
        !validRtpStream(
            output.audio, synchronization.audioOutput,
            *runtime.planningFacts.outputAudioRtpPacketization,
            outer.audioPlan.resolvedOutput->codecName(),
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
        outer.outputLayout !=
            RealtimeOutputStreamLayout::MuxedTransportStream ||
        outer.outputTransport != MediaOutputTransportKind::UdpDatagrams ||
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
    if (output.url.empty() ||
        output.resourceKind != MediaOutputResourceKind::ByteSink ||
        output.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
        output.protocol.audioSampleRate() !=
            runtime.audioCorrection.outputSampleRate ||
        output.protocol.muxPlan().parameters() !=
            runtime.synchronization.projectMpegTsOutput->outputMux
                ->parameters()) {
        return invalidOutput("Project MPEG-TS protocol facts");
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

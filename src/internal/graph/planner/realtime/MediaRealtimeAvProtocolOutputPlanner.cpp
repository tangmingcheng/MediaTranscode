#include "internal/graph/planner/realtime/MediaRealtimeAvProtocolOutputPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeDatagramTransportPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/planner/realtime/MediaRtcpReportingPolicyPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaScheduledRtpOutputPlan> scheduledRtpOutput(
    MediaScheduledStream stream,
    MediaRealtimeScheduledRtpOutputPlanningDraft& plannedOutput,
    const MediaAvSyncRtpOutputStreamPlan& synchronization,
    MediaRunningTime senderLead,
    const MediaRealtimeDeploymentEnvelope& deployment,
    std::uint64_t sessionBandwidthBytesPerSecond,
    std::string bandwidthAuthority)
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
    const auto& endpoint = plannedOutput.scheduledTransport->remoteRtpEndpoint();
    auto address = MediaNumericIpAddress::create(
        endpoint.addressFamily(), endpoint.numericAddress());
    auto compoundWire = MediaRtcpWireGeometry::compoundWireBytes(
        synchronization.cname->size(), endpoint.addressFamily());
    auto reporting = address && compoundWire
        ? MediaRtcpReportingPolicyPlanner::plan(
              deployment, address.value(), sessionBandwidthBytesPerSecond,
              std::move(bandwidthAuthority), compoundWire.value())
        : ::media::Result<MediaRtcpReportingPolicy>::failure(
              !address ? address.error() : compoundWire.error());
    if (!reporting) {
        return ::media::Result<MediaScheduledRtpOutputPlan>::failure(
            reporting.error());
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
            std::move(reporting).value()});
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

::media::Result<MediaRealtimeAvProtocolOutputPlan>
MediaRealtimeAvProtocolOutputPlanner::plan(
    const MediaRealtimeAvProtocolOutputRequest& request,
    MediaRealtimeOutputPlanningDraft& output)
{
    if (!request.groupKey.valid() || !request.emission.audio) {
        return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V protocol output requires its group identity and prepared audio emission"));
    }
    MediaAvSyncOutputAdapterKind adapter;
    std::optional<MediaRunningTime> activationOutputLead;
    std::optional<std::variant<MediaSeparateRtpOutputRuntimePlan,
                               MediaProjectMpegTsRuntimeOutputPlan>> protocolOutput;
    if (request.rtp) {
        if (request.mpegTs ||
            request.layout != RealtimeOutputStreamLayout::SeparateStreams ||
            request.transport != MediaOutputTransportKind::RtpAvp ||
            !request.outputLead ||
            output.sdp.path.empty()) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP synchronization output facts are incomplete"));
        }
        auto video = scheduledRtpOutput(
            MediaScheduledStream::Video,
            output.videoOutput,
            request.rtp->videoOutput,
            *request.outputLead,
            request.deployment,
            request.emission.video.sustainedPayloadBytesPerSecond,
            request.emission.video.authority);
        auto audio = scheduledRtpOutput(
            MediaScheduledStream::Audio,
            output.audioOutput,
            request.rtp->audioOutput,
            *request.outputLead,
            request.deployment,
            request.emission.audio->sustainedPayloadBytesPerSecond,
            request.emission.audio->authority);
        if (!video || !audio) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                video ? audio.error() : video.error());
        }
        auto sdp = scheduledSdp(
            output, video.value(), audio.value());
        if (!sdp) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                sdp.error());
        }
        adapter = MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp;
        activationOutputLead = *request.outputLead;
        protocolOutput.emplace(std::in_place_type<MediaSeparateRtpOutputRuntimePlan>,
            MediaSeparateRtpOutputRuntimePlan{
                std::move(video).value(),
                std::move(audio).value(),
                std::move(sdp).value()});
    } else if (request.mpegTs) {
        if (request.rtp ||
            request.layout !=
                RealtimeOutputStreamLayout::MuxedTransportStream ||
            !request.mpegTs->outputMux ||
            !request.audioSampleRate || output.muxedOutput.url.empty()) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS synchronization output facts are incomplete"));
        }
        auto accepted = MediaProjectMpegTsOutputPlan::fromAudioVideoEncodedFacts(
            *request.mpegTs->outputMux);
        if (!accepted) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                accepted.error());
        }
        if (!request.videoFrameRate.isKnown() || request.videoFrameRate.num <= 0 ||
            request.videoFrameRate.den <= 0 ||
            !request.audioSampleRate ||
            !request.audioBatchSamples ||
            *request.audioBatchSamples <= 0) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS emission requires planner-owned output cadences"));
        }
        auto videoCadence = MediaRunningTime::checkedFromTicks(
            1, request.videoFrameRate.den, request.videoFrameRate.num);
        auto audioCadence = MediaRunningTime::checkedFromTicks(
            *request.audioBatchSamples,
            1, *request.audioSampleRate);
        if (!videoCadence || !audioCadence) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                videoCadence ? audioCadence.error() : videoCadence.error());
        }
        auto projectActivationLead =
            accepted.value().muxPlan().transportDecodeLead().checkedAdd(
                accepted.value().muxPlan().startupEmissionPreroll());
        if (!projectActivationLead) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                projectActivationLead.error());
        }
        activationOutputLead = projectActivationLead.value();
        std::optional<std::variant<
            MediaMpegTsUdpOutputPlan,
            MediaMpegTsRtpOutputPlan>> transport;
        if (request.transport ==
            MediaOutputTransportKind::UdpDatagrams) {
            if (output.muxedOutput.rtpTransport ||
                !output.muxedOutput.sdpPath.empty() ||
                accepted.value().muxPlan().parameters().transportKind !=
                    MediaOutputTransportKind::UdpDatagrams) {
                return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS UDP output rejects RTP transport facts"));
            }
            transport.emplace(
                std::in_place_type<MediaMpegTsUdpOutputPlan>,
                MediaMpegTsUdpOutputPlan{
                    output.muxedOutput.url,
                    MediaOutputResourceKind::ByteSink,
                    MediaMuxSessionKind::ProjectMpegTs});
        } else if (request.transport ==
                   MediaOutputTransportKind::RtpAvp) {
            if (!output.muxedOutput.rtpTransport ||
                !output.muxedOutput.maximumDatagramBytes ||
                output.muxedOutput.sdpPath.empty() ||
                output.muxedOutput.mediaId.empty() ||
                accepted.value().muxPlan().parameters().transportKind !=
                    MediaOutputTransportKind::RtpAvp) {
                return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MPEG-TS RTP output requires complete planned transport facts"));
            }
            auto sessionBandwidth = MediaCheckedArithmetic::add(
                request.emission.video.sustainedPayloadBytesPerSecond,
                request.emission.audio->sustainedPayloadBytesPerSecond,
                "MPEG-TS RTP session media bandwidth");
            const auto& endpoint =
                output.muxedOutput.rtpTransport->remoteRtpEndpoint();
            auto address = MediaNumericIpAddress::create(
                endpoint.addressFamily(), endpoint.numericAddress());
            const auto cname = MediaRtpOutputIdentityPlanner::cname(
                output.muxedOutput.mediaId);
            auto compoundWire = MediaRtcpWireGeometry::compoundWireBytes(
                cname.size(), endpoint.addressFamily());
            auto reporting = sessionBandwidth && address && compoundWire
                ? MediaRtcpReportingPolicyPlanner::plan(
                      request.deployment, address.value(),
                      sessionBandwidth.value(),
                      request.emission.video.authority + "+" +
                          request.emission.audio->authority,
                      compoundWire.value())
                : ::media::Result<MediaRtcpReportingPolicy>::failure(
                      !sessionBandwidth ? sessionBandwidth.error() :
                      !address ? address.error() : compoundWire.error());
            if (!reporting) {
                return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                    reporting.error());
            }
            auto rtp = MediaMpegTsRtpOutputPlan::create(
                std::move(*output.muxedOutput.rtpTransport),
                *output.muxedOutput.maximumDatagramBytes,
                output.muxedOutput.sdpPath,
                output.muxedOutput.mediaId,
                std::move(reporting).value());
            if (!rtp ||
                rtp.value().tsPacketsPerPayload() !=
                    accepted.value().muxPlan().parameters()
                        .maximumPacketsPerDatagram) {
                return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                    rtp ? ::media::ErrorInfo::invalidArgument(
                              "MPEG-TS RTP transport batching differs from the mux plan")
                        : rtp.error());
            }
            transport.emplace(
                std::in_place_type<MediaMpegTsRtpOutputPlan>,
                std::move(rtp).value());
        } else {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                ::media::ErrorInfo::unsupported(
                    "MPEG-TS output transport is unsupported"));
        }
        adapter = MediaAvSyncOutputAdapterKind::ProjectMpegTs;
        const std::uint64_t maximumQueuedBytes =
            request.maximumQueuedPacketBytes;
        auto emission = MediaTsDatagramEmissionPlan::create(
            accepted.value().muxPlan(), videoCadence.value(),
            audioCadence.value(), maximumQueuedBytes,
            request.deployment.encode().latency.targetResidence);
        if (!emission) {
            return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
                emission.error());
        }
        protocolOutput.emplace(std::in_place_type<MediaProjectMpegTsRuntimeOutputPlan>,
            MediaProjectMpegTsRuntimeOutputPlan{
                std::move(accepted).value(),
                MediaMuxSessionKind::ProjectMpegTs,
                std::move(emission).value(),
                request.transport == MediaOutputTransportKind::RtpAvp
                    ? request.maximumQueuedPacketBytes
                    : 0,
                std::move(*transport)});
    } else {
        return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V runtime output authority is unsupported"));
    }

    if (!activationOutputLead) {
        return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V runtime output requires planner-owned activation lead"));
    }
    auto datagramTransport = std::visit(
        [&](const auto& selectedOutput) {
            return MediaRealtimeDatagramTransportPlanner::plan(
                request.groupKey.value(), request.deployment, selectedOutput, request.emission);
        },
        *protocolOutput);
    if (!datagramTransport) {
        return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::failure(
            datagramTransport.error());
    }
    return ::media::Result<MediaRealtimeAvProtocolOutputPlan>::success({
        adapter, *activationOutputLead, std::move(*protocolOutput),
        std::move(datagramTransport).value()});
}

} // namespace media::ffmpeg::graph

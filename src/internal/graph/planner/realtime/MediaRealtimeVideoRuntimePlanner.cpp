#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanner.h"

#include "internal/graph/planner/capability/MediaSelectedEncoderPacketLayoutResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeDatagramTransportPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t VideoStartupMaximumWaitNs = 10'000'000'000;
constexpr std::int64_t SenderReportIntervalNs = 1'000'000'000;
constexpr int VideoRtpPayloadType = 96;
constexpr int VideoRtpClockRate = 90'000;
constexpr std::uint64_t InitialVideoGeneration = 1;

::media::Result<MediaVideoOnlySeparateRtpOutputRuntimePlan>
planSeparateRtp(
    MediaRealtimeOutputPlanningDraft& output,
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!output.videoOutput.scheduledTransport ||
        !output.videoOutput.scheduledPacketization ||
        output.sdp.path.empty() || !request.deployment) {
        return ::media::Result<
            MediaVideoOnlySeparateRtpOutputRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly scheduled RTP requires complete transport, packetization, and SDP facts"));
    }
    const std::string& identity = request.mediaId;
    const std::string cname = MediaRtpOutputIdentityPlanner::cname(identity);
    const auto& endpoint =
        output.videoOutput.scheduledTransport->remoteRtpEndpoint();
    auto sdpIdentity = MediaSdpSessionIdentity::create(
        identity, 0, 0, identity, endpoint.addressFamily(),
        endpoint.numericAddress(), cname);
    if (!sdpIdentity) {
        return ::media::Result<
            MediaVideoOnlySeparateRtpOutputRuntimePlan>::failure(
            sdpIdentity.error());
    }
    MediaSeparateRtpSdpRuntimePlan sdp{
        output.sdp.path,
        identity,
        identity,
        endpoint.addressFamily(),
        endpoint.numericAddress(),
        cname,
        MediaRtpSdpSessionIdPolicy::SharedNtpEpoch,
        MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration};
    MediaScheduledRtpOutputPlan video{
        MediaScheduledStream::Video,
        std::move(*output.videoOutput.scheduledTransport),
        *output.videoOutput.scheduledPacketization,
        MediaRtpOutputIdentityPlanner::stableFfmpegMuxSsrc(
            identity + ".output.video"),
        MediaRtpOutputIdentityPlanner::stableNumeric(
            identity + ".video.timestamp"),
        VideoRtpClockRate,
        cname,
        request.deployment->encode().latency.maximumResidence,
        MediaRunningTime::fromNanoseconds(SenderReportIntervalNs)};
    return ::media::Result<
        MediaVideoOnlySeparateRtpOutputRuntimePlan>::success(
        MediaVideoOnlySeparateRtpOutputRuntimePlan{
            std::move(video),
            std::move(sdp)});
}

::media::Result<MediaProjectMpegTsRuntimeOutputPlan> planProjectMpegTs(
    MediaRealtimeRtpTranscodePlanningDraft& outer,
    MediaRealtimeOutputPlanningDraft& output,
    const MediaRealtimeRtpTranscodeRequest& request,
    MediaRational outputFrameRate,
    const MediaRealtimeVideoStartupPlan& startup)
{
    auto layout = MediaSelectedEncoderPacketLayoutResolver::resolve(
        outer.videoPlan);
    if (!layout) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(
            layout.error());
    }
    if (output.muxedOutput.url.empty()) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly Project MPEG-TS requires a planned output URL"));
    }
    if (!request.deployment || !output.muxedOutput.maximumDatagramBytes) {
        return ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly MPEG-TS requires deployment MTU authority"));
    }
    std::uint16_t maximumPacketsPerDatagram = 0;
    if (outer.outputTransport == MediaOutputTransportKind::RtpAvp ||
        outer.outputTransport == MediaOutputTransportKind::UdpDatagrams) {
        auto packetCount = MediaTsMuxPlan::maximumPacketsPerDatagram(
            *output.muxedOutput.maximumDatagramBytes,
            outer.outputTransport);
        if (!packetCount) {
            return ::media::Result<
                MediaProjectMpegTsRuntimeOutputPlan>::failure(
                packetCount.error());
        }
        maximumPacketsPerDatagram = packetCount.value();
    } else if (outer.outputTransport !=
               MediaOutputTransportKind::UdpDatagrams) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "VideoOnly Project MPEG-TS transport is unsupported"));
    }
    if (!output.muxedOutput.transportDecodeLead ||
        !output.muxedOutput.startupEmissionPreroll) {
        return ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly MPEG-TS output requires a planned transport decode lead"));
    }
    auto videoCadence = MediaRunningTime::checkedFromTicks(
        1, outputFrameRate.den, outputFrameRate.num);
    if (!videoCadence) {
        return ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::failure(
            videoCadence.error());
    }
    auto protocol = MediaProjectMpegTsOutputPlan::createVideoOnly(
        outer.videoPlan.outputCodecName, layout.value(),
        *output.muxedOutput.transportDecodeLead,
        *output.muxedOutput.startupEmissionPreroll,
        outer.outputTransport, maximumPacketsPerDatagram);
    if (!protocol) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(protocol.error());
    }
    std::optional<std::variant<MediaMpegTsUdpOutputPlan,
                               MediaMpegTsRtpOutputPlan>> transport;
    if (outer.outputTransport == MediaOutputTransportKind::UdpDatagrams) {
        if (output.muxedOutput.rtpTransport ||
            !output.muxedOutput.sdpPath.empty()) {
            return ::media::Result<
                MediaProjectMpegTsRuntimeOutputPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoOnly MPEG-TS/UDP rejects RTP transport facts"));
        }
        transport.emplace(
            std::in_place_type<MediaMpegTsUdpOutputPlan>,
            MediaMpegTsUdpOutputPlan{
                output.muxedOutput.url,
                MediaOutputResourceKind::ByteSink,
                MediaMuxSessionKind::ProjectMpegTs});
    } else {
        if (!output.muxedOutput.rtpTransport ||
            !output.muxedOutput.maximumDatagramBytes ||
            output.muxedOutput.sdpPath.empty()) {
            return ::media::Result<
                MediaProjectMpegTsRuntimeOutputPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "VideoOnly MPEG-TS/RTP requires complete RTP and SDP facts"));
        }
        auto rtp = MediaMpegTsRtpOutputPlan::create(
            std::move(*output.muxedOutput.rtpTransport),
            *output.muxedOutput.maximumDatagramBytes,
            output.muxedOutput.sdpPath,
            request.mediaId,
            MediaRunningTime::fromNanoseconds(SenderReportIntervalNs));
        if (!rtp || rtp.value().tsPacketsPerPayload() !=
                        maximumPacketsPerDatagram) {
            return ::media::Result<
                MediaProjectMpegTsRuntimeOutputPlan>::failure(
                rtp ? ::media::ErrorInfo::invalidArgument(
                          "VideoOnly MPEG-TS/RTP batching differs from its mux product")
                    : rtp.error());
        }
        transport.emplace(
            std::in_place_type<MediaMpegTsRtpOutputPlan>,
            std::move(rtp).value());
    }
    outer.videoParameters.globalHeader = true;
    auto emission = MediaTsDatagramEmissionPlan::create(
        protocol.value().muxPlan(), videoCadence.value(), std::nullopt,
        startup.byteCapacity,
        request.deployment->encode().latency.targetResidence);
    if (!emission) {
        return ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::failure(
            emission.error());
    }
    return ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::success(
        MediaProjectMpegTsRuntimeOutputPlan{
            std::move(protocol).value(),
            MediaMuxSessionKind::ProjectMpegTs,
            std::move(emission).value(),
            outer.outputTransport == MediaOutputTransportKind::RtpAvp
                ? startup.byteCapacity
                : 0,
            std::move(*transport)});
}

::media::Result<MediaRealtimeVideoStartupPlan> planStartup(
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    auto capacity = MediaRealtimeMediaCapacityPlanner::plan(ledger);
    if (!capacity || capacity.value().audioUnits) {
        return ::media::Result<MediaRealtimeVideoStartupPlan>::failure(
            capacity ? ::media::ErrorInfo::invalidArgument(
                           "VideoOnly runtime rejects audio capacity")
                     : capacity.error());
    }
    return ::media::Result<MediaRealtimeVideoStartupPlan>::success(
        MediaRealtimeVideoStartupPlan{
            true,
            MediaRunningTime::fromNanoseconds(VideoStartupMaximumWaitNs),
            capacity.value().videoUnits,
            capacity.value().videoUnitBytes,
            capacity.value().videoBytes});
}

void applyStartupMemoryBounds(
    MediaEdgePolicy& policy,
    const MediaRealtimeVideoStartupPlan& startup) noexcept
{
    auto& memory = policy.bufferPolicy.memoryBudget;
    memory.maxBytes = startup.byteCapacity;
    memory.softLimitBytes = startup.byteCapacity;
    memory.reservedBytes = 0;
    memory.maxBuffers = startup.packetCapacity;
    memory.preallocatedBuffers = 0;
    memory.enforceHardLimit = true;
    memory.allowDynamicGrowth = false;
}

MediaVideoLineageEdgePolicySet planLineageEdges(
    const MediaRealtimeEdgePolicySet& policies,
    const MediaRealtimeVideoStartupPlan& startup)
{
    MediaVideoLineageEdgePolicySet lineage{
        policies.synchronizedPacket,
        policies.atomicVideoPacket,
        policies.synchronizedVideoFrame,
        policies.preparedVideoFrame};
    applyStartupMemoryBounds(lineage.ingressPacket, startup);
    applyStartupMemoryBounds(lineage.startupPacket, startup);
    return lineage;
}

} // namespace

::media::Result<MediaRealtimeVideoRuntimePlan>
MediaRealtimeVideoRuntimePlanner::plan(
    MediaRealtimeRtpTranscodePlanningDraft& outer,
    MediaRealtimeOutputPlanningDraft output,
    const MediaRealtimeRtpTranscodeRequest& request,
    MediaRational sourceTimeBase,
    MediaRational outputFrameRate)
{
    if (!sourceTimeBase.isKnown() || sourceTimeBase.num <= 0 ||
        sourceTimeBase.den <= 0 || !outputFrameRate.isKnown() ||
        outputFrameRate.num <= 0 || outputFrameRate.den <= 0) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly runtime requires planner-selected source timing and output cadence"));
    }
    if (request.mediaId.empty()) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly runtime requires an explicit media identity"));
    }
    if (!outer.resourceLedger) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly runtime requires the graph resource ledger"));
    }
    auto startup = planStartup(*outer.resourceLedger);
    if (!startup) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            startup.error());
    }
    MediaRealtimeVideoPacketTimingMode packetTimingMode;
    MediaRational scheduledPacketTimeBase;
    if (outer.videoPlan.branchMode == MediaBranchMode::CopyPacket) {
        packetTimingMode =
            MediaRealtimeVideoPacketTimingMode::PacketDuration;
        scheduledPacketTimeBase = sourceTimeBase;
    } else if (outer.videoPlan.branchMode ==
               MediaBranchMode::TranscodeFrame) {
        packetTimingMode =
            MediaRealtimeVideoPacketTimingMode::PlannedCadence;
        scheduledPacketTimeBase = MediaRational{
            outputFrameRate.den, outputFrameRate.num};
    } else {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "VideoOnly runtime requires an enabled video pipeline"));
    }

    auto plannedEdges = MediaRealtimeEdgePolicyPlanner::
        planWithSynchronizedPacketMemoryBudget(
            outer.queues, startup.value().byteCapacity,
            startup.value().packetCapacity);
    if (!plannedEdges) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            plannedEdges.error());
    }
    MediaRealtimeEdgePolicySet edgePolicies =
        std::move(plannedEdges).value();
    MediaVideoLineageEdgePolicySet lineageEdgePolicies =
        planLineageEdges(edgePolicies, startup.value());

    std::optional<MediaRealtimeVideoOutputAdapterPlan> adapter;
    std::optional<MediaRunningTime> activationOutputLead;
    std::optional<MediaRunningTime> transportLead;
    if (outer.outputLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        auto planned = planSeparateRtp(output, request);
        if (!planned) {
            return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
                planned.error());
        }
        activationOutputLead = planned.value().video.senderLead;
        transportLead = planned.value().video.senderLead;
        adapter.emplace(
            std::in_place_type<MediaVideoOnlySeparateRtpOutputRuntimePlan>,
            std::move(planned).value());
    } else if (outer.outputLayout ==
               RealtimeOutputStreamLayout::MuxedTransportStream) {
        auto planned = planProjectMpegTs(
            outer, output, request, outputFrameRate, startup.value());
        if (!planned) {
            return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
                planned.error());
        }
        auto projectActivationLead =
            planned.value().protocol.muxPlan().transportDecodeLead().checkedAdd(
                planned.value().protocol.muxPlan().startupEmissionPreroll());
        if (!projectActivationLead) {
            return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
                projectActivationLead.error());
        }
        activationOutputLead = projectActivationLead.value();
        transportLead =
            planned.value().protocol.muxPlan().transportDecodeLead();
        adapter.emplace(
            std::in_place_type<MediaProjectMpegTsRuntimeOutputPlan>,
            std::move(planned).value());
    } else {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "VideoOnly runtime output layout is unsupported"));
    }

    if (!request.deployment) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly Datagram transport requires deployment facts"));
    }
    auto datagramTransport = std::visit(
        [&](const auto& plannedOutput) {
            using Output = std::decay_t<decltype(plannedOutput)>;
            if constexpr (std::is_same_v<
                              Output,
                              MediaProjectMpegTsRuntimeOutputPlan>) {
                return MediaRealtimeDatagramTransportPlanner::plan(
                    request.mediaId, *request.deployment, plannedOutput,
                    outer.videoPlan, outputFrameRate, nullptr);
            } else {
                return MediaRealtimeDatagramTransportPlanner::plan(
                    request.mediaId, *request.deployment, plannedOutput,
                    outer.videoPlan, outputFrameRate);
            }
        },
        *adapter);
    if (!datagramTransport) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            datagramTransport.error());
    }
    return ::media::Result<MediaRealtimeVideoRuntimePlan>::success(
        MediaRealtimeVideoRuntimePlan{
            std::move(startup).value(),
            MediaRealtimeVideoTimingPlan{
                sourceTimeBase,
                outputFrameRate,
                scheduledPacketTimeBase,
                packetTimingMode,
                MediaRealtimeVideoTimestampAuthority::DecodeTimestamp},
            MediaRealtimeVideoSchedulingPlan{
                true, *activationOutputLead, *transportLead,
                InitialVideoGeneration},
            MediaProtocolOutputSessionKey(request.mediaId),
            output.packetCopyNormalizationRequired,
            std::move(*adapter),
            std::move(datagramTransport).value(),
            outer.queues,
            std::move(edgePolicies),
            std::move(lineageEdgePolicies),
            outer.threadingPolicy});
}

} // namespace media::ffmpeg::graph

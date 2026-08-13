#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanner.h"

#include "internal/graph/planner/capability/MediaSelectedEncoderPacketLayoutResolver.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include <limits>
#include <optional>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t VideoStartupMaximumWaitNs = 10'000'000'000;
constexpr std::int64_t ProtocolOutputLeadNs = 100'000'000;
constexpr std::int64_t ProjectTsStartupPrerollNs = 40'000'000;
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
        output.sdp.path.empty()) {
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
        MediaRunningTime::fromNanoseconds(ProtocolOutputLeadNs),
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
    const MediaRealtimeRtpTranscodeRequest& request)
{
    auto layout = MediaSelectedEncoderPacketLayoutResolver::resolve(
        outer.videoPlan);
    if (!layout || outer.videoPlan.outputCodecName != "h264" ||
        output.muxedOutput.url.empty()) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(
            layout ? ::media::ErrorInfo::unsupported(
                         "VideoOnly Project MPEG-TS requires planner-selected H.264 transcode output")
                   : layout.error());
    }
    std::uint8_t maximumPacketsPerDatagram = 7;
    if (outer.outputTransport == MediaOutputTransportKind::RtpAvp) {
        if (!request.output.packetSize || *request.output.packetSize <= 0) {
            return ::media::Result<
                MediaProjectMpegTsRuntimeOutputPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "VideoOnly MPEG-TS/RTP requires an explicit datagram size"));
        }
        auto packetCount = MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
            static_cast<std::size_t>(*request.output.packetSize));
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
    auto protocol = MediaProjectMpegTsOutputPlan::createVideoOnly(
        outer.videoPlan.outputCodecName, layout.value(),
        MediaRunningTime::fromNanoseconds(ProtocolOutputLeadNs),
        MediaRunningTime::fromNanoseconds(ProjectTsStartupPrerollNs),
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
            output.muxedOutput.sdpPath.empty()) {
            return ::media::Result<
                MediaProjectMpegTsRuntimeOutputPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "VideoOnly MPEG-TS/RTP requires complete RTP and SDP facts"));
        }
        auto rtp = MediaMpegTsRtpOutputPlan::create(
            std::move(*output.muxedOutput.rtpTransport),
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
    return ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::success(
        MediaProjectMpegTsRuntimeOutputPlan{
            std::move(protocol).value(),
            MediaMuxSessionKind::ProjectMpegTs,
            std::move(*transport)});
}

::media::Result<MediaRealtimeVideoStartupPlan> planStartup(
    const MediaRealtimeRtpTranscodePlanningDraft& outer,
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (outer.queues.packet == 0 ||
        !request.avSyncStartup.maximumVideoUnitBytes ||
        *request.avSyncStartup.maximumVideoUnitBytes == 0) {
        return ::media::Result<MediaRealtimeVideoStartupPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly runtime requires explicit video startup bounds"));
    }
    const auto capacity = static_cast<std::uint64_t>(outer.queues.packet);
    const auto unitBytes = static_cast<std::uint64_t>(
        *request.avSyncStartup.maximumVideoUnitBytes);
    if (capacity > std::numeric_limits<std::uint64_t>::max() / unitBytes ||
        unitBytes > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()) ||
        capacity * unitBytes > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        return ::media::Result<MediaRealtimeVideoStartupPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly startup byte capacity is not representable"));
    }
    return ::media::Result<MediaRealtimeVideoStartupPlan>::success(
        MediaRealtimeVideoStartupPlan{
            true,
            MediaRunningTime::fromNanoseconds(VideoStartupMaximumWaitNs),
            outer.queues.packet,
            unitBytes,
            capacity * unitBytes});
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
    auto startup = planStartup(outer, request);
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

    MediaRealtimeEdgePolicySet edgePolicies = outer.edgePolicies;
    applyStartupMemoryBounds(
        edgePolicies.synchronizedPacket, startup.value());
    MediaVideoLineageEdgePolicySet lineageEdgePolicies =
        planLineageEdges(edgePolicies, startup.value());

    std::optional<MediaRealtimeVideoOutputAdapterPlan> adapter;
    std::optional<MediaRunningTime> transportLead;
    if (outer.outputLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        auto planned = planSeparateRtp(output, request);
        if (!planned) {
            return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
                planned.error());
        }
        transportLead = planned.value().video.senderLead;
        adapter.emplace(
            std::in_place_type<MediaVideoOnlySeparateRtpOutputRuntimePlan>,
            std::move(planned).value());
    } else if (outer.outputLayout ==
               RealtimeOutputStreamLayout::MuxedTransportStream) {
        auto planned = planProjectMpegTs(outer, output, request);
        if (!planned) {
            return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
                planned.error());
        }
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
                true, *transportLead, InitialVideoGeneration},
            MediaProtocolOutputSessionKey(request.mediaId),
            output.packetCopyNormalizationRequired,
            std::move(*adapter),
            outer.queues,
            std::move(edgePolicies),
            std::move(lineageEdgePolicies),
            outer.threadingPolicy});
}

} // namespace media::ffmpeg::graph

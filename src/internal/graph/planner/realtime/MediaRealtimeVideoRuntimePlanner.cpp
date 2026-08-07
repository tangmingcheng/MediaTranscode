#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t VideoStartupMaximumWaitNs = 10'000'000'000;

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

} // namespace

::media::Result<MediaRealtimeVideoRuntimePlan>
MediaRealtimeVideoRuntimePlanner::plan(
    const MediaRealtimeRtpTranscodePlanningDraft& outer,
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
    auto startup = planStartup(outer, request);
    if (!startup) {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            startup.error());
    }
    MediaRealtimeVideoPacketTimingMode packetTimingMode;
    MediaRational scheduledPacketTimeBase;
    if (outer.videoPlan.branchMode == MediaBranchMode::CopyPacket) {
        packetTimingMode =
            MediaRealtimeVideoPacketTimingMode::SourceTimeBase;
        scheduledPacketTimeBase = sourceTimeBase;
    } else if (outer.videoPlan.branchMode ==
               MediaBranchMode::TranscodeFrame) {
        packetTimingMode =
            MediaRealtimeVideoPacketTimingMode::OutputCadenceTimeBase;
        scheduledPacketTimeBase = MediaRational{
            outputFrameRate.den, outputFrameRate.num};
    } else {
        return ::media::Result<MediaRealtimeVideoRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "VideoOnly runtime requires an enabled video pipeline"));
    }

    MediaRealtimeEdgePolicySet edgePolicies = outer.edgePolicies;
    auto& memory =
        edgePolicies.synchronizedPacket.bufferPolicy.memoryBudget;
    memory.maxBytes = startup.value().byteCapacity;
    memory.softLimitBytes = startup.value().byteCapacity;
    memory.reservedBytes = 0;
    memory.maxBuffers = startup.value().packetCapacity;
    memory.preallocatedBuffers = 0;
    memory.enforceHardLimit = true;
    memory.allowDynamicGrowth = false;

    output.singleStreamMux.pacingPolicy.enablePacing = false;
    output.singleStreamMux.startupDelayMs = 0;
    std::optional<MediaRealtimeVideoOutputAdapterPlan> adapter;
    if (outer.outputLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        adapter.emplace(
            std::in_place_type<MediaRealtimeVideoSeparateRtpAdapterPlan>,
            MediaRealtimeVideoSeparateRtpAdapterPlan{
                output.packetCopyNormalizationRequired,
                std::move(output.videoOutput),
                std::move(output.sdp),
                std::move(output.singleStreamMux)});
    } else if (outer.outputLayout ==
               RealtimeOutputStreamLayout::MuxedTransportStream) {
        adapter.emplace(
            std::in_place_type<MediaRealtimeVideoMuxedAdapterPlan>,
            MediaRealtimeVideoMuxedAdapterPlan{
                output.packetCopyNormalizationRequired,
                std::move(output.muxedOutput),
                std::move(output.singleStreamMux)});
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
                true, MediaRunningTime::fromNanoseconds(0)},
            std::move(*adapter),
            outer.queues,
            std::move(edgePolicies),
            outer.threadingPolicy});
}

} // namespace media::ffmpeg::graph

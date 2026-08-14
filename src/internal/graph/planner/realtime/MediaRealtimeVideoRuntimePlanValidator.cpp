#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {

::media::Status MediaRealtimeVideoRuntimePlanValidator::validate(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeVideoRuntimePlan& runtime)
{
    const auto invalid = [](const char* field) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            std::string("Invalid VideoOnly runtime product: ") + field));
    };
    if (!runtime.startup.requireKeyFrame ||
        runtime.startup.maximumWait <=
            MediaRunningTime::fromNanoseconds(0) ||
        runtime.startup.packetCapacity == 0 ||
        runtime.startup.maximumUnitBytes == 0 ||
        runtime.startup.packetCapacity != runtime.queues.packet ||
        runtime.startup.packetCapacity >
            std::numeric_limits<std::uint64_t>::max() /
                runtime.startup.maximumUnitBytes ||
        runtime.startup.byteCapacity !=
            static_cast<std::uint64_t>(runtime.startup.packetCapacity) *
                runtime.startup.maximumUnitBytes) {
        return invalid("startup policy");
    }
    if (!runtime.timing.sourceTimeBase.isKnown() ||
        runtime.timing.sourceTimeBase.num <= 0 ||
        runtime.timing.sourceTimeBase.den <= 0 ||
        !runtime.timing.outputFrameRate.isKnown() ||
        runtime.timing.outputFrameRate.num <= 0 ||
        runtime.timing.outputFrameRate.den <= 0 ||
        !runtime.timing.scheduledPacketTimeBase.isKnown() ||
        runtime.timing.scheduledPacketTimeBase.num <= 0 ||
        runtime.timing.scheduledPacketTimeBase.den <= 0 ||
        runtime.timing.timestampAuthority !=
            MediaRealtimeVideoTimestampAuthority::DecodeTimestamp) {
        return invalid("timing authority");
    }
    const bool sourceTiming = outer.videoPlan.branchMode ==
            MediaBranchMode::CopyPacket &&
        runtime.timing.packetTimingMode ==
            MediaRealtimeVideoPacketTimingMode::PacketDuration &&
        runtime.timing.scheduledPacketTimeBase.num ==
            runtime.timing.sourceTimeBase.num &&
        runtime.timing.scheduledPacketTimeBase.den ==
            runtime.timing.sourceTimeBase.den;
    const bool cadenceTiming = outer.videoPlan.branchMode ==
            MediaBranchMode::TranscodeFrame &&
        runtime.timing.packetTimingMode ==
            MediaRealtimeVideoPacketTimingMode::PlannedCadence &&
        runtime.timing.scheduledPacketTimeBase.num ==
            runtime.timing.outputFrameRate.den &&
        runtime.timing.scheduledPacketTimeBase.den ==
            runtime.timing.outputFrameRate.num;
    if (!sourceTiming && !cadenceTiming) {
        return invalid("scheduled packet timing derivation");
    }
    if (!runtime.scheduling.pacingEnabled ||
        runtime.scheduling.transportLead <=
            MediaRunningTime::fromNanoseconds(0) ||
        runtime.scheduling.initialGeneration == 0 ||
        runtime.scheduling.initialGeneration >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()) ||
        !runtime.sessionKey.valid()) {
        return invalid("scheduling policy");
    }
    MediaRealtimeEdgePolicySet expectedEdges =
        MediaRealtimeEdgePolicyPlanner::plan(runtime.queues);
    const auto applyStartupMemoryBounds = [&](MediaEdgePolicy& policy) {
        auto& memory = policy.bufferPolicy.memoryBudget;
        memory.maxBytes = runtime.startup.byteCapacity;
        memory.softLimitBytes = runtime.startup.byteCapacity;
        memory.reservedBytes = 0;
        memory.maxBuffers = runtime.startup.packetCapacity;
        memory.preallocatedBuffers = 0;
        memory.enforceHardLimit = true;
        memory.allowDynamicGrowth = false;
    };
    applyStartupMemoryBounds(expectedEdges.synchronizedPacket);
    MediaVideoLineageEdgePolicySet expectedLineage{
        expectedEdges.synchronizedPacket,
        expectedEdges.atomicVideoPacket,
        expectedEdges.synchronizedVideoFrame,
        expectedEdges.preparedVideoFrame};
    applyStartupMemoryBounds(expectedLineage.startupPacket);
    if (runtime.edgePolicies != expectedEdges ||
        runtime.lineageEdgePolicies != expectedLineage ||
        runtime.lineageEdgePolicies.ingressPacket.queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer ||
        !MediaAtomicOutputPolicyContract::accepts(
            runtime.lineageEdgePolicies.startupPacket) ||
        runtime.lineageEdgePolicies.frame.queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer ||
        !MediaAtomicOutputPolicyContract::accepts(
            runtime.lineageEdgePolicies.preparedFrame) ||
        runtime.threadingPolicy.mode != MediaThreadingMode::PerNodeWorker ||
        runtime.threadingPolicy.priority != MediaThreadPriority::High ||
        runtime.threadingPolicy.maxWorkerThreads != 0 ||
        runtime.threadingPolicy.pinWorkers ||
        !runtime.threadingPolicy.collectWorkerMetrics) {
        return invalid("queue, edge, or threading product");
    }
    if (outer.outputLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        const auto* output =
            std::get_if<MediaVideoOnlySeparateRtpOutputRuntimePlan>(
                &runtime.outputAdapter);
        if (!output || output->sdp.path.empty() ||
            output->video.stream != MediaScheduledStream::Video ||
            output->video.senderLead != runtime.scheduling.transportLead ||
            output->video.senderLead <= MediaRunningTime::fromNanoseconds(0) ||
            output->video.senderReportInterval <=
                MediaRunningTime::fromNanoseconds(0) ||
            output->video.clockRate != 90'000 ||
            output->video.ssrc == 0 || output->video.cname.empty() ||
            output->video.cname != output->sdp.cname ||
            output->video.packetization.streamKind() !=
                MediaStreamKind::Video) {
            return invalid("separate RTP adapter");
        }
    } else if (outer.outputLayout ==
               RealtimeOutputStreamLayout::MuxedTransportStream) {
        const auto* output = std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(
            &runtime.outputAdapter);
        const auto expectedEmission = output
            ? MediaTsDatagramEmissionPlan::create(
                  output->protocol.muxPlan(),
                  output->emission.videoInitialServiceWindow(),
                  output->emission.audioInitialServiceWindow())
            : ::media::Result<MediaTsDatagramEmissionPlan>::failure(
                  ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS output is absent"));
        if (!output ||
            output->muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
            !output->protocol.muxPlan().videoOnlyProgram() ||
            output->protocol.muxPlan().audioVideoProgram() ||
            output->protocol.muxPlan().transportDecodeLead() !=
                runtime.scheduling.transportLead ||
            !expectedEmission ||
            output->emission != expectedEmission.value() ||
            output->protocol.muxPlan().parameters().transportKind !=
                outer.outputTransport) {
            return invalid("muxed adapter");
        }
    } else {
        return invalid("output layout");
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

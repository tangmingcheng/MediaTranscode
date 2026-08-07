#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

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
    if (outer.audioPlan.enabled ||
        outer.audioPlan.branchMode != MediaBranchMode::Drop ||
        outer.useIsolatedAudioInput || outer.avSyncComponentBounds) {
        return invalid("audio or A/V product is present");
    }
    if (!runtime.startup.requireKeyFrame ||
        runtime.startup.maximumWait <=
            MediaRunningTime::fromNanoseconds(0) ||
        runtime.startup.packetCapacity == 0 ||
        runtime.startup.maximumUnitBytes == 0 ||
        runtime.startup.packetCapacity != outer.queues.packet ||
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
        runtime.timing.timestampAuthority !=
            MediaRealtimeVideoTimestampAuthority::DecodeTimestamp) {
        return invalid("timing authority");
    }
    if (!runtime.scheduling.pacingEnabled ||
        runtime.scheduling.transportLead !=
            MediaRunningTime::fromNanoseconds(0)) {
        return invalid("scheduling policy");
    }
    if (runtime.queues.metadata != outer.queues.metadata ||
        runtime.queues.packet != outer.queues.packet ||
        runtime.queues.frame != outer.queues.frame ||
        runtime.queues.mux != outer.queues.mux ||
        runtime.edgePolicies !=
            MediaRealtimeEdgePolicyPlanner::plan(runtime.queues) ||
        runtime.threadingPolicy.mode != MediaThreadingMode::PerNodeWorker ||
        runtime.threadingPolicy.priority != MediaThreadPriority::High ||
        runtime.threadingPolicy.maxWorkerThreads != 0 ||
        runtime.threadingPolicy.pinWorkers ||
        !runtime.threadingPolicy.collectWorkerMetrics) {
        return invalid("queue, edge, or threading product");
    }
    if (outer.outputLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        const auto* output =
            std::get_if<MediaRealtimeVideoSeparateRtpAdapterPlan>(
                &runtime.outputAdapter);
        if (!output || output->output.url.empty() ||
            output->output.packetSize <= 0 || output->sdp.path.empty() ||
            !output->mux.expectVideo || output->mux.expectAudio ||
            output->mux.pacingPolicy.enablePacing ||
            output->mux.startupDelayMs != 0) {
            return invalid("separate RTP adapter");
        }
    } else if (outer.outputLayout ==
               RealtimeOutputStreamLayout::MuxedTransportStream) {
        const auto* output = std::get_if<MediaRealtimeVideoMuxedAdapterPlan>(
            &runtime.outputAdapter);
        if (!output || output->output.url.empty() ||
            output->output.format.empty() ||
            !output->output.muxSessionKind || !output->mux.expectVideo ||
            output->mux.expectAudio || output->mux.pacingPolicy.enablePacing ||
            output->mux.startupDelayMs != 0) {
            return invalid("muxed adapter");
        }
    } else {
        return invalid("output layout");
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

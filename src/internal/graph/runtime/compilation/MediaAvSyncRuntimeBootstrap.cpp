#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaAvSyncClockBundle>
MediaAvSyncRuntimeBootstrap::createClocks(
    const MediaAvSyncRuntimeBinding& binding,
    MediaAvSyncClockSource& source)
{
    if (!binding.groupKey.valid()) {
        return ::media::Result<MediaAvSyncClockBundle>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync runtime binding requires a valid group key"));
    }
    if (auto status = MediaAvSyncPlanValidator::validate(binding.plan);
        !status) {
        return ::media::Result<MediaAvSyncClockBundle>::failure(
            status.error());
    }
    const bool requireSharedNtpEpoch =
        *binding.plan.topology ==
        MediaAvSyncTopology::SeparateRtpToSeparateRtp;
    auto clocks = source.capture(requireSharedNtpEpoch);
    if (!clocks) return clocks;
    if (!clocks.value().masterClock ||
        static_cast<bool>(clocks.value().sharedNtpEpoch) !=
            requireSharedNtpEpoch) {
        return ::media::Result<MediaAvSyncClockBundle>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync clock source violated the planned clock bundle"));
    }
    return clocks;
}

::media::Status MediaAvSyncRuntimeBootstrap::registerGroup(
    const MediaAvSyncRuntimeBinding& binding,
    MediaAvSyncClockBundle clocks,
    MediaGraphExecutionContext& context)
{
    if (!context.compiled()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync runtime bootstrap requires a compiled context"));
    }
    return context.registerAvSyncGroup(
        binding.groupKey, binding.plan, std::move(clocks.masterClock),
        std::move(clocks.sharedNtpEpoch));
}

} // namespace media::ffmpeg::graph

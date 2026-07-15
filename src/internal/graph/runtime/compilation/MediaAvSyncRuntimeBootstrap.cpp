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

::media::Result<MediaPlaybackEpochActivationCapability>
MediaAvSyncRuntimeBootstrap::registerGroupAndIssueActivationCapability(
    const MediaAvSyncRuntimeBinding& binding,
    MediaAvSyncClockBundle clocks,
    MediaGraphExecutionContext& context)
{
    auto transition = MediaAvEpochTransitionService::create(binding.transition);
    if (!transition) {
        return ::media::Result<MediaPlaybackEpochActivationCapability>::failure(
            transition.error());
    }
    auto service = std::move(transition).value();
    auto registered = context.registerAvSyncGroup(
        binding.groupKey, binding.plan, std::move(clocks.masterClock),
        std::move(clocks.sharedNtpEpoch), service);
    if (!registered) {
        return ::media::Result<MediaPlaybackEpochActivationCapability>::failure(
            registered.error());
    }
    return ::media::Result<MediaPlaybackEpochActivationCapability>::success(
        MediaPlaybackEpochActivationCapability(service));
}

} // namespace media::ffmpeg::graph

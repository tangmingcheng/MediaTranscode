#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaAvSyncSharedNtpEpochRequirement.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaAvSyncClockBundle>
MediaAvSyncRuntimeBootstrap::createClocks(
    const MediaAvSyncRuntimeBinding& binding,
    MediaAvSyncClockSource& source)
{
    if (binding.domains.empty() || !binding.outputGroupKey.valid()) {
        return ::media::Result<MediaAvSyncClockBundle>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync runtime requires domains and an explicit output group"));
    }
    bool requireSharedNtpEpoch = false;
    for (const auto& domain : binding.domains) {
        if (!domain.groupKey.valid()) {
            return ::media::Result<MediaAvSyncClockBundle>::failure(
                ::media::ErrorInfo::invalidArgument("A/V runtime domain requires a valid group"));
        }
        if (auto status = MediaAvSyncPlanValidator::validateRuntime(domain.plan); !status) {
            return ::media::Result<MediaAvSyncClockBundle>::failure(status.error());
        }
        auto requirement = MediaAvSyncSharedNtpEpochRequirement::resolve(domain.plan);
        if (!requirement) return ::media::Result<MediaAvSyncClockBundle>::failure(requirement.error());
        requireSharedNtpEpoch = requireSharedNtpEpoch || requirement.value();
    }
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

::media::Result<MediaAvSyncRuntimeBootstrap::Activation>
MediaAvSyncRuntimeBootstrap::registerGroupAndIssueActivationCapability(
    const MediaAvRuntimeDomainBinding& binding,
    MediaAvSyncClockBundle clocks,
    MediaGraphExecutionContext& context)
{
    using Result = ::media::Result<Activation>;
    std::shared_ptr<MediaAvEpochTransitionService> service;
    const auto* shared = std::get_if<MediaAvSharedSourceOutputDomainBinding>(&binding.role);
    const auto* source = std::get_if<MediaAvSourceDomainBinding>(&binding.role);
    if (!binding.plan.sourceLifecycle ||
        (source && binding.plan.sourceLifecycle->mode != MediaAvSourceLifecycleMode::PreserveActivatedOutput) ||
        (!source && binding.plan.sourceLifecycle->mode != MediaAvSourceLifecycleMode::FailSessionOnSourceLoss))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "A/V source lifecycle differs from its planned domain role"));
    if (shared || source) {
        auto transition = MediaAvEpochTransitionService::create(
            shared ? shared->transition : source->transition);
        if (!transition) return Result::failure(transition.error());
        service = std::move(transition).value();
    } else {
        service = MediaAvEpochTransitionService::createInitialOnly();
    }
    auto requirement = MediaAvSyncSharedNtpEpochRequirement::resolve(binding.plan);
    if (!requirement) return Result::failure(requirement.error());
    if (!requirement.value()) clocks.sharedNtpEpoch.reset();
    auto registered = context.registerAvSyncGroup(
        binding.groupKey, binding.plan, std::move(clocks.masterClock),
        std::move(clocks.sharedNtpEpoch), service);
    if (!registered) return Result::failure(registered.error());
    if (shared || source) {
        return Result::success(Activation(MediaPlaybackEpochActivationCapability(service)));
    }
    return Result::success(Activation(MediaOutputEpochActivationCapability(service)));
}

::media::Result<MediaAvReacquisitionAssemblyDependencies>
MediaAvSyncRuntimeBootstrap::reacquisitionAssemblyDependencies(
    const MediaPlaybackEpochActivationCapability& capability,
    const std::shared_ptr<MediaAvSyncGroupRuntime>& group)
{
    auto transition = capability.m_transition.lock();
    if (!transition || !transition->transitionPlan() || !group || !group->clock()) {
        return ::media::Result<
            MediaAvReacquisitionAssemblyDependencies>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V reacquisition assembly requires the registered transition and master clock"));
    }
    return ::media::Result<
        MediaAvReacquisitionAssemblyDependencies>::success(
        MediaAvReacquisitionAssemblyDependencies{
            std::move(transition), group->clock()});
}

} // namespace media::ffmpeg::graph

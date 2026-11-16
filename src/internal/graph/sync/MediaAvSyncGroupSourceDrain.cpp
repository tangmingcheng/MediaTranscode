#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {
::media::Status MediaAvSyncGroupRuntime::beginSourceDrain(MediaAvSourceDrainRequest request)
{
    if (m_plan.domainRole != MediaAvSyncDomainRole::SourceContribution ||
        !m_plan.sourceLifecycle ||
        m_plan.sourceLifecycle->mode != MediaAvSourceLifecycleMode::PreserveActivatedOutput)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source drain requires the planned continuous source contribution lifecycle"));
    return m_transitionService->beginSourceDrain(std::move(request));
}
::media::Status MediaAvSyncGroupRuntime::sealSourceDrainInput(std::uint64_t generation,
    MediaStreamKind stream, std::uint64_t ingressGeneration, std::uint64_t acceptedFrontier)
{
    return m_transitionService->sealSourceDrainInput(generation, stream, ingressGeneration, acceptedFrontier);
}
::media::Result<bool> MediaAvSyncGroupRuntime::acknowledgeSourceDrained(
    std::uint64_t generation, MediaStreamKind stream)
{
    return m_transitionService->acknowledgeSourceDrained(generation, stream);
}
std::optional<MediaAvSourceDrainSnapshot> MediaAvSyncGroupRuntime::sourceDrainSnapshot() const noexcept
{
    return m_transitionService->sourceDrainSnapshot();
}
::media::Status MediaAvSyncGroupRuntime::completeSourceDrain(std::uint64_t generation)
{
    const auto state = sourceDrainSnapshot();
    if (!state || state->request.playbackGeneration != generation || !state->complete())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source drain completion requires all sealed members to finish"));
    return requestReacquisition({generation, MediaAvReacquisitionReason::SourceDrained});
}
} // namespace media::ffmpeg::graph

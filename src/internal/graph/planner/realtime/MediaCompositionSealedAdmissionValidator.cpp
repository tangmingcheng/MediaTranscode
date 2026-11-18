#include "internal/graph/planner/realtime/MediaCompositionSealedAdmissionValidator.h"

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
}
::media::Status MediaCompositionSealedAdmissionValidator::validate(
    MediaCompositionCandidateWorkspace& workspace,
    std::span<const MediaCompositionSourceFixedRequest> prepared,
    std::span<const MediaCompositionSourcePreparedFacts> sealed,
    std::span<const MediaCompositionSourceCapacityCandidate> selected)
{
    if (!workspace.valid() || sealed.empty() || sealed.size() != workspace.sources.size() ||
        prepared.size() != sealed.size() || selected.size() != sealed.size())
        return invalid("Sealed composition requires the complete selected workspace");
    for (std::size_t i = 0; i < sealed.size(); ++i) {
        const auto& source = workspace.sources[i];
        const auto& view = sealed[i].source;
        const auto* admission = view.continuousAdmission;
        const auto* capacity = std::get_if<MediaAvSourceCapacityFacts>(&sealed[i].capacity);
        if (!admission || !capacity || !source.selectedCapacity || !admission->owner || !admission->budgets.metadata || !admission->budgets.videoPayload ||
            admission->owner != view.continuousVideoResource ||
            !view.continuousStorage || !view.continuousStorage->sourceBudgets ||
            admission->budgets != *view.continuousStorage->sourceBudgets ||
            !admission->owner->usesBudget(*admission->budgets.metadata) ||
            source.runtime.groupKey.value() != sealed[i].original.sourceRequest.mediaId ||
            source.runtime.rtpControlDelivery != prepared[i].rtpControlDelivery ||
            &source.video != &sealed[i].original.video ||
            !source.selectedCapacity->sameProduct(*capacity, source.runtime.edgePolicies.metadata,
                source.runtime.edgePolicies.atomicMetadata) ||
            !source.selectedCapacity->sameProduct(selected[i].capacity, source.runtime.edgePolicies.metadata,
                source.runtime.edgePolicies.atomicMetadata) ||
            admission->video.bounds != selected[i].containers.video ||
            admission->audio.has_value() != selected[i].containers.audio.has_value() ||
            (admission->audio && admission->audio->bounds != *selected[i].containers.audio))
            return invalid("Sealed source changed selected capacity, identity or container bounds");
        const auto videoBudget = admission->budgets.videoPayload->snapshot();
        const auto metadataBudget = admission->budgets.metadata->snapshot();
        if (!videoBudget.sealed || !metadataBudget.sealed ||
            videoBudget.guaranteedBytes != admission->video.credit.guaranteedPayloadBytes)
            return invalid("Sealed video credit differs from its actual budget guarantee");
        if (admission->audio) {
            if (!admission->budgets.audioPayload)
                return invalid("Sealed audio payload budget is absent");
            const auto audioBudget = admission->budgets.audioPayload->snapshot();
            if (!audioBudget.sealed ||
                audioBudget.guaranteedBytes != admission->audio->credit.guaranteedPayloadBytes)
                return invalid("Sealed audio credit differs from its actual budget guarantee");
        }
    }
    // Equality above establishes that selected quantities are the independent
    // sealed readback. Reuse their existing backing; do not copy route strings.
    if (auto status = workspace.rebind(prepared, selected); !status) return status;
    for (std::size_t i = 0; i < sealed.size(); ++i) {
        const auto& admission = *sealed[i].source.continuousAdmission;
        const auto& cost = workspace.costs[i];
        if (admission.fixedBytes != cost.fixedBytes || admission.protocolBytes != cost.protocolBytes ||
            admission.startupBytes != cost.startupBytes || admission.totalBytes != cost.totalBytes ||
            admission.video.storage != cost.video.containers || admission.video.credit != cost.video.credit ||
            admission.audio.has_value() != cost.audio.has_value() ||
            (admission.audio && (admission.audio->storage != cost.audio->containers ||
                admission.audio->credit != cost.audio->credit)))
            return invalid("Sealed composition differs from independently recomputed storage costs");
    }
    return ::media::Status::success();
}
}

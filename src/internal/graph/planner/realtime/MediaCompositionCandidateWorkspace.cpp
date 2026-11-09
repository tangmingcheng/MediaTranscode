#include "internal/graph/planner/realtime/MediaCompositionCandidateWorkspace.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
}
MediaCompositionCandidateWorkspace::MediaCompositionCandidateWorkspace(
    MediaFixedCapacityStorage<MediaCompositionSourcePlanningProducts> sourceProducts,
    std::shared_ptr<MediaPreparationMemoryResource> resource, MediaCompositionOutputResidence outputResidence)
    : m_resource(std::move(resource)), m_outputResidence(outputResidence),
      sources(std::move(sourceProducts)), costs(sources.size(), *m_resource),
      m_held(sources.size(), *m_resource), m_aggregateFacts(sources.size(), *m_resource), m_topologyFacts(sources.size(), *m_resource),
      m_residenceFacts(sources.size(), *m_resource)
{
}

::media::Status MediaCompositionCandidateWorkspace::project(
    std::span<const MediaCompositionSourceFixedRequest> prepared,
    std::span<const MediaCompositionSourceCapacityCandidate> capacities, bool initial)
{
    if (sources.empty() || prepared.size() != sources.size() || capacities.size() != sources.size())
        return invalid("Composition workspace source membership changed");
    m_aggregateFacts.clear();
    m_topologyFacts.clear();
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto& source = sources[i];
        const auto& capacity = capacities[i].capacity;
        const auto& input = prepared[i];
        const bool audio = source.runtime.audioPipeline.enabled;
        const auto* transport = source.input.rtpTransport ? &*source.input.rtpTransport : nullptr;
        const auto* audioTransport = source.runtime.isolatedAudioInput && source.runtime.isolatedAudioInput->rtpTransport
            ? &*source.runtime.isolatedAudioInput->rtpTransport : nullptr;
        if (source.planningResource != m_resource || &source.video != &input.video ||
            source.runtime.groupKey.value() != input.sourceRequest.mediaId ||
            !transport || !transport->ingress || !transport->ingress->sameProduct(input.videoIngress) ||
            (audio && (!audioTransport || !audioTransport->ingress || !input.audioIngress ||
                !audioTransport->ingress->sameProduct(*input.audioIngress))) ||
            !source.selectedCapacity || !source.selectedCapacity->sameProduct(capacity,
                source.runtime.edgePolicies.metadata, source.runtime.edgePolicies.atomicMetadata) ||
            audio != (input.audioIngress != nullptr) ||
            audio != capacity.startup.audioUnits.has_value() || audio != capacities[i].containers.audio.has_value() ||
            capacity.videoAcquiringCapacity != capacities[i].containers.video.acquiringObjects ||
            (audio && capacity.audioAcquiringCapacity != capacities[i].containers.audio->acquiringObjects))
            return invalid("Composition workspace differs from selected source capacity or ingress");
        m_aggregateFacts.push_back(MediaAvContinuousAggregatePlanner::sourceFacts(source));
        m_topologyFacts.push_back({source.runtime.synchronization, source.runtime.edgePolicies,
            source.branchConnections, source.inputConnections});
        if (initial) {
            m_residenceFacts.push_back({capacity.route, capacity.startup.videoUnits, capacity.startup.audioUnits,
                capacity.videoAcquiringCapacity, capacity.audioAcquiringCapacity,
                input.videoIngress.descriptorCapacity(),
                audio ? std::optional<std::uint64_t>{input.audioIngress->descriptorCapacity()} : std::nullopt,
                source.producerHolders.filterRetainedInputFrames});
        } else {
            auto& facts = m_residenceFacts[i];
            if (facts.route.shape.members != capacity.route.shape.members ||
                facts.route.shape.readiness != capacity.route.shape.readiness ||
                facts.route.metadata.bufferPolicy.sharedAllocationGroup != capacity.route.metadata.bufferPolicy.sharedAllocationGroup ||
                facts.route.atomicMetadata.bufferPolicy.sharedAllocationGroup != capacity.route.atomicMetadata.bufferPolicy.sharedAllocationGroup ||
                facts.videoDescriptorCapacity != input.videoIngress.descriptorCapacity() ||
                facts.audioDescriptorCapacity != (audio ? std::optional<std::uint64_t>{input.audioIngress->descriptorCapacity()} : std::nullopt) ||
                facts.filterRetainedInputs != source.producerHolders.filterRetainedInputFrames)
                return invalid("Composition residence fixed declarations changed");
            facts.route = capacity.route;
            facts.videoBatchUnits = capacity.startup.videoUnits;
            facts.audioBatchUnits = capacity.startup.audioUnits;
            facts.videoAcquiringCapacity = capacity.videoAcquiringCapacity;
            facts.audioAcquiringCapacity = capacity.audioAcquiringCapacity;
        }
    }
    return ::media::Status::success();
}

::media::Status MediaCompositionCandidateWorkspace::price(
    std::span<const MediaCompositionSourceCapacityCandidate> capacities, bool initial)
{
    m_sourceBytes = 0;
    if (!aggregate || !topology || m_held.size() != sources.size())
        return invalid("Composition workspace lacks complete topology or held snapshots");
    const MediaCompositionResidenceFacts facts{m_residenceFacts.view(), m_outputResidence.videoFrames,
        m_outputResidence.audioFrames, aggregate->storage.maximumRetainedObjects};
    if (initial) {
        auto planned = MediaCompositionPayloadResidencePlanner::compile(topology->graph, facts);
        if (!planned) return ::media::Status::failure(planned.error());
        residence.emplace(std::move(planned).value());
    } else {
        if (!residence) return invalid("Composition workspace lost reference residence");
        if (auto status = MediaCompositionPayloadResidencePlanner::rebind(topology->graph, facts, *residence); !status)
            return status;
    }
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (!m_held[i].plan) return invalid("Composition workspace has no source session plan");
        const auto& source = sources[i];
        const auto& candidate = capacities[i];
        const MediaPreparedSourceCandidateCostFacts costFacts{{candidate.containers.video},
            candidate.containers.audio ? std::optional{MediaPreparedStreamCandidateCostFacts{*candidate.containers.audio}} : std::nullopt,
            candidate.capacity.startup.videoUnits, candidate.capacity.startup.audioUnits, source.video,
            topology->graph, *residence, i, source.lineageRegistryCapacity, candidate.capacity.route,
            source.runtime.groupKey.value().size(), source.producerHolders};
        if (initial) {
            auto planned = MediaPreparedSourceCandidateCostPlanner::plan(*m_held[i].plan, m_held[i].snapshot, costFacts);
            if (!planned) return ::media::Status::failure(planned.error());
            costs.push_back(std::move(planned).value());
        } else if (auto status = MediaPreparedSourceCandidateCostPlanner::rebind(
                *m_held[i].plan, m_held[i].snapshot, costFacts, costs[i]); !status) return status;
        auto total = MediaCheckedArithmetic::add(m_sourceBytes, costs[i].totalBytes, "composition candidate source total");
        if (!total) return ::media::Status::failure(total.error());
        if (total.value() > (std::numeric_limits<std::size_t>::max)())
            return invalid("Composition candidate source total exceeds address space");
        m_sourceBytes = static_cast<std::size_t>(total.value());
    }
    return ::media::Status::success();
}

::media::Result<MediaCompositionCandidateWorkspace> MediaCompositionCandidateWorkspace::create(
    MediaFixedCapacityStorage<MediaCompositionSourcePlanningProducts> sourceProducts,
    std::span<const MediaCompositionSourceFixedRequest> prepared,
    std::span<const MediaCompositionSourceCapacityCandidate> capacities,
    std::span<const MediaPreparedRtpStorageSourceSnapshot> held,
    const MediaRealtimeAvOutputRuntimePlan& output, const MediaVideoCanvasGeometry& canvas,
    MediaRational cadence, std::size_t audioSource, const MediaPreparedRealtimeEmissionSet& emission,
    const MediaRealtimeDeploymentLatencyBudget& latency, std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    using Result = ::media::Result<MediaCompositionCandidateWorkspace>;
    if (!resource || sourceProducts.empty() || sourceProducts.resource() != resource.get() ||
        capacities.size() != sourceProducts.size() || prepared.size() != sourceProducts.size() || held.size() != sourceProducts.size())
        return Result::failure(::media::ErrorInfo::invalidArgument("Composition workspace requires exact budgeted source products"));
    if (auto status = MediaStandardContainerStorageReadback::validatePreparationMoves(); !status)
        return Result::failure(status.error());
    auto outputRetention = MediaRealtimeCompositionResourcePlanner::outputResidence(emission, latency);
    if (!outputRetention) return Result::failure(outputRetention.error());
    std::size_t backing = 0;
    for (const auto& candidate : capacities) {
        for (const auto* policy : {&candidate.capacity.route.metadata, &candidate.capacity.route.atomicMetadata}) {
            auto bytes = MediaStandardContainerStorageReadback::stringCopy(policy->bufferPolicy.sharedAllocationGroup.size());
            if (!bytes) return Result::failure(bytes.error());
            if (bytes.value() > (std::numeric_limits<std::size_t>::max)() - backing)
                return Result::failure(::media::ErrorInfo::invalidArgument("Composition residence backing overflows"));
            backing += bytes.value();
        }
    }
    MediaCompositionCandidateWorkspace result(std::move(sourceProducts), std::move(resource), outputRetention.value());
    if (backing) {
        auto reserved = result.m_resource->reserveStorage(backing, MediaStorageAdmission::RequiredAllocation);
        if (!reserved) return Result::failure(reserved.error());
        result.m_routeBacking.emplace(std::move(reserved).value());
    }
    for (const auto& snapshot : held) {
        if (!snapshot.plan || !snapshot.snapshot.owner)
            return Result::failure(::media::ErrorInfo::invalidArgument("Composition workspace requires live session snapshots"));
        for (const auto& existing : result.m_held)
            if (existing.plan == snapshot.plan || existing.snapshot.owner == snapshot.snapshot.owner)
                return Result::failure(::media::ErrorInfo::invalidArgument("Composition workspace rejects duplicate source owners"));
        result.m_held.push_back(snapshot);
    }
    if (auto status = result.project(prepared, capacities, true); !status) return Result::failure(status.error());
    auto aggregatePlan = MediaAvContinuousAggregatePlanner::plan(result.m_aggregateFacts.view(), output,
        canvas, cadence, audioSource, *result.m_resource);
    if (!aggregatePlan) return Result::failure(aggregatePlan.error());
    result.aggregate.emplace(std::move(aggregatePlan).value());
    auto topologyPlan = MediaCompositionTopologyPlanningKernel::plan(result.m_topologyFacts.view(),
        *result.aggregate, output, result.m_resource);
    if (!topologyPlan) return Result::failure(topologyPlan.error());
    result.topology.emplace(std::move(topologyPlan).value());
    if (auto status = result.price(capacities, true); !status) return Result::failure(status.error());
    result.m_valid = true;
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionCandidateWorkspace>::failure(
        ::media::ErrorInfo::allocationFailed("Composition candidate workspace"));
}

::media::Status MediaCompositionCandidateWorkspace::rebind(
    std::span<const MediaCompositionSourceFixedRequest> prepared,
    std::span<const MediaCompositionSourceCapacityCandidate> capacities)
{
    const bool wasValid = m_valid;
    m_valid = false;
    m_sourceBytes = 0;
    if (!wasValid || !aggregate || !topology || !residence || prepared.size() != sources.size() || capacities.size() != sources.size() ||
        m_held.size() != sources.size() || costs.size() != sources.size() || m_residenceFacts.size() != sources.size())
        return invalid("Composition refresh requires a complete valid workspace");
    for (std::size_t i = 0; i < sources.size(); ++i)
        if (auto status = MediaCompositionSourcePlanningKernel::rebindCapacity(prepared[i], capacities[i].capacity, sources[i]); !status)
            return status;
    if (auto status = project(prepared, capacities, false); !status) return status;
    if (auto status = MediaAvContinuousAggregatePlanner::rebindCapacity(m_aggregateFacts.view(), *aggregate); !status) return status;
    if (auto status = MediaCompositionTopologyPlanningKernel::rebind(m_topologyFacts.view(), *aggregate, *topology); !status) return status;
    if (auto status = price(capacities, false); !status) return status;
    m_valid = true;
    return ::media::Status::success();
}
}

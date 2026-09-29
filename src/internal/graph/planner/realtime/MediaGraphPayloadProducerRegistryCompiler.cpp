#include "internal/graph/planner/realtime/MediaGraphPayloadProducerRegistryCompiler.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadProducerContract.h"
#include "internal/graph/planner/realtime/MediaGraphFrameCreditContractPlanner.h"
#include <algorithm>
#include <utility>
#include <new>

namespace media::ffmpeg::graph {
namespace {
template<class Left, class Right>
bool sameKey(const Left& left, const Right& right) noexcept
{
    return left.nodeId == right.nodeId && left.streamKind == right.streamKind &&
        left.payloadKind == right.payloadKind;
}
}

::media::Result<MediaGraphPayloadCreditPlan> MediaGraphPayloadProducerRegistryCompiler::compile(
    const MediaGraph& graph, std::span<const MediaGraphPayloadProducerFact> facts,
    std::uint64_t availablePayloadBytes, std::uint64_t maximumPayloadObjects,
    std::span<const MediaNodeId> selectedNodes)
try {
    using Result = ::media::Result<MediaGraphPayloadCreditPlan>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    if (graph.empty() || availablePayloadBytes == 0 || maximumPayloadObjects == 0)
        return invalid("payload producer registry requires a graph and positive global credits");
    const auto selected = [&](MediaNodeId id) {
        return selectedNodes.empty() || std::find(selectedNodes.begin(), selectedNodes.end(), id) != selectedNodes.end();
    };
    for (std::size_t i = 0; i < selectedNodes.size(); ++i) {
        if (!graph.findNode(selectedNodes[i]) ||
            std::find(selectedNodes.begin(), selectedNodes.begin() + i, selectedNodes[i]) != selectedNodes.begin() + i)
            return invalid("payload producer selection contains a missing or duplicate node");
    }
    MediaGraphPayloadCreditPlan plan;
    plan.maximumBytes = availablePayloadBytes;
    plan.maximumObjects = maximumPayloadObjects;
    plan.producerStrategyVersion = 1;
    plan.integration = MediaGraphPayloadCreditIntegration::Complete;
    plan.authority = "final-dag-producer-registry+prepared-emission+global-payload-budget";
    for (std::size_t index = 0; index < facts.size(); ++index) {
        const auto& fact = facts[index];
        const auto* node = graph.findNode(fact.nodeId);
        if (!node || !selected(node->id) || !mediaGraphPayloadProducerOutput(node->kind))
            return invalid("payload fact does not belong to a selected integrated producer");
        for (std::size_t previous = 0; previous < index; ++previous)
            if (sameKey(facts[previous], fact)) return invalid("duplicate payload producer fact");
        if (fact.payloadKind != MediaPayloadKind::Packet && fact.payloadKind != MediaPayloadKind::Frame)
            return invalid("payload producer fact must describe a packet or frame");
        const bool outputExists = std::any_of(graph.edges().begin(), graph.edges().end(), [&](const auto& edge) {
            return edge.from.nodeId == fact.nodeId && edge.streamKind == fact.streamKind && edge.payloadKind == fact.payloadKind;
        });
        if (!outputExists) return invalid("payload producer fact has no matching graph output");
        if (!acceptsMediaGraphPayloadProducerOutput(node->kind, fact.streamKind, fact.payloadKind))
            return invalid("payload producer output conflicts with its runtime contract");
        if (fact.payloadKind == MediaPayloadKind::Frame) {
            auto expected = MediaGraphFrameCreditContractPlanner::plan(*node, fact.maximumLogicalBytes);
            if (!expected) return Result::failure(expected.error());
            if (!fact.frameCredit || fact.frameCredit->allocationScope != expected.value().allocationScope ||
                fact.frameCredit->maximumLogicalBytes != expected.value().maximumLogicalBytes ||
                fact.frameCredit->maximumObjectsPerAllocation != expected.value().maximumObjectsPerAllocation ||
                fact.frameCredit->authority != expected.value().authority)
                return invalid("prepared frame credit differs from the final node contract");
        }
        const bool device = fact.frameCredit && fact.frameCredit->allocationScope ==
            MediaFrameCreditAllocationScope::ExternalDeviceObservedOnly;
        MediaGraphPayloadProducerStrategy strategy{
            fact.nodeId, fact.streamKind, fact.payloadKind,
            device ? MediaGraphPayloadAllocationAccounting::ObservedOnlyExternalBytesAndEngineManagedObject
                   : MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject,
            fact.frameCredit, fact.maximumLogicalBytes, true, fact.authority};
        if (!strategy.valid() || (fact.streamKind != MediaStreamKind::Audio && fact.streamKind != MediaStreamKind::Video))
            return invalid("payload producer fact lacks a valid logical bound and frame contract");
        if (!device && fact.maximumLogicalBytes > availablePayloadBytes)
            return invalid("single producer payload exceeds the global credit pool");
        plan.maximumUnitBytes = (std::max)(plan.maximumUnitBytes, fact.maximumLogicalBytes);
        plan.producers.push_back(std::move(strategy));
    }
    // Cover actual output keys, not merely the facts supplied by a caller.
    for (const auto& node : graph.nodes()) {
        if (!selected(node.id) || !mediaGraphPayloadProducerOutput(node.kind)) continue;
        bool foundOutput = false;
        for (const auto& edge : graph.edges()) {
            if (edge.from.nodeId != node.id ||
                (edge.payloadKind != MediaPayloadKind::Packet && edge.payloadKind != MediaPayloadKind::Frame)) continue;
            foundOutput = true;
            if (!std::any_of(facts.begin(), facts.end(), [&](const auto& fact) {
                    return fact.nodeId == node.id && fact.streamKind == edge.streamKind && fact.payloadKind == edge.payloadKind;
                })) return invalid("final DAG payload producer is missing its own prepared fact");
        }
        if (!foundOutput) return invalid("final DAG payload producer has no typed packet/frame output");
    }
    if (!plan.isCompleteAndValid()) return invalid("payload producer registry is incomplete");
    return Result::success(std::move(plan));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaGraphPayloadCreditPlan>::failure(
        ::media::ErrorInfo::allocationFailed("payload producer registry"));
}

} // namespace media::ffmpeg::graph

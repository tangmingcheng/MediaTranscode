#include "internal/graph/planner/realtime/MediaRealtimeInitialOutputResourcePartition.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>
#include <charconv>

namespace media::ffmpeg::graph {
namespace {
using Arithmetic = MediaCheckedArithmetic;

// A producer's lease follows shared references through every node of the same
// payload kind. A downstream producer may alias its input; it is not a cut.
::media::Status boundProducerResidence(
    const MediaGraph& graph,
    const MediaFinalGraphResourceLedger& complete,
    const MediaRealtimeGraphResourceLedgerPlan& planning,
    MediaFinalGraphResourceLedger& segment)
{
    auto& credits = segment.payloadCreditPlan;
    std::uint64_t bytes = 0;
    std::uint64_t objects = 0;
    for (const auto& producer : credits.producers) {
        std::vector<MediaNodeId> reachable{producer.nodeId};
        const auto contains = [&](MediaNodeId id) {
            return std::find(reachable.begin(), reachable.end(), id) != reachable.end();
        };
        for (std::size_t index = 0; index < reachable.size(); ++index) {
            const auto current = reachable[index];
            for (const auto& edge : graph.edges()) {
                if (edge.from.nodeId == current &&
                    edge.payloadKind == producer.payloadKind &&
                    edge.streamKind == producer.streamKind && !contains(edge.to.nodeId)) {
                    reachable.push_back(edge.to.nodeId);
                }
            }
        }
        std::uint64_t retained = 0;
        for (const auto& entry : complete.entries) {
            bool belongs = false;
            if (entry.owner.starts_with("node:")) {
                for (const auto id : reachable) {
                    const auto* node = graph.findNode(id);
                    if (node && entry.owner == "node:" + node->name) {
                        belongs = true;
                        break;
                    }
                }
            } else if (entry.owner.starts_with("edge:")) {
                for (const auto& edge : graph.edges()) {
                    if (contains(edge.from.nodeId) &&
                        edge.payloadKind == producer.payloadKind &&
                        edge.streamKind == producer.streamKind &&
                        entry.owner == "edge:" + edge.name) {
                        belongs = true;
                        break;
                    }
                }
            }
            if (!belongs) continue;
            auto sum = Arithmetic::add(retained, entry.maximumBufferObjects,
                                       "producer reachable retention credits");
            if (!sum) return ::media::Status::failure(sum.error());
            retained = sum.value();
        }
        const auto* node = graph.findNode(producer.nodeId);
        if (!node) return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("allocation budget lost its producer"));
        std::uint64_t ownedBytes = 0;
        std::string authority;
        if (producer.accounting == MediaGraphPayloadAllocationAccounting::
                ObservedOnlyExternalBytesAndEngineManagedObject) {
            authority = "hardware-payload-observed-only+bounded-engine-reference-objects";
        } else if (producer.payloadKind == MediaPayloadKind::Packet) {
            if (node->kind == MediaNodeKind::VideoEncode) {
                ownedBytes = planning.media.videoBytes;
                authority = "prepared-encoder-emission-residence-allocation-quota";
            } else if (node->kind == MediaNodeKind::RawRtpInput ||
                       node->kind == MediaNodeKind::Demux ||
                       node->kind == MediaNodeKind::MpegTsDemux) {
                if (!planning.preparedInputPayload) return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized("input allocation quota lacks prepared envelope"));
                const MediaNode* decoder = nullptr;
                for (const auto id : reachable) {
                    const auto* candidate = graph.findNode(id);
                    if (candidate && candidate->kind == MediaNodeKind::VideoDecode) {
                        if (decoder) return ::media::Status::failure(::media::ErrorInfo::unsupported(
                            "input allocation quota requires a single shared decoder"));
                        decoder = candidate;
                    }
                }
                if (!decoder) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "input allocation quota has no reachable shared decoder"));
                const auto internalText = decoder->options.value(
                    "decoder.pipeline.input_retention.maximum_internal_packets");
                std::uint64_t internalPackets = 0;
                const auto parsed = std::from_chars(internalText.data(),
                    internalText.data() + internalText.size(), internalPackets);
                if (parsed.ec != std::errc{} || parsed.ptr != internalText.data() + internalText.size() ||
                    internalPackets == 0) return ::media::Status::failure(::media::ErrorInfo::unsupported(
                    "selected decoder lacks an authoritative compressed-input retention adapter"));
                // VideoDecodeNode has one pendingPacket slot during send EAGAIN.
                // It can coexist with codec-owned packets and a new input batch.
                auto retainedPackets = Arithmetic::add(internalPackets, 1,
                    "decoder internal packets and runtime pending input slot");
                if (!retainedPackets) return ::media::Status::failure(retainedPackets.error());
                auto allocations = Arithmetic::add(retainedPackets.value(),
                    planning.preparedInputPayload->maximumPayloadsPerInputCompletion,
                    "input atomic completion and decoder in-flight allocations");
                if (!allocations) return ::media::Status::failure(allocations.error());
                auto quota = Arithmetic::multiply(producer.maximumReservedBytes(),
                    allocations.value(), "prepared input and decoder retention allocation quota");
                mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::GraphBuild,
                    "input_allocation_slots atomic_batch=" + std::to_string(
                        planning.preparedInputPayload->maximumPayloadsPerInputCompletion) +
                    " node_pending=1 decoder_internal=" + std::to_string(internalPackets) +
                    " authority=" + decoder->options.value("decoder.pipeline.input_retention.authority"));
                if (!quota) return ::media::Status::failure(quota.error());
                ownedBytes = quota.value();
                authority = "prepared-input-atomic-completion+opened-decoder-retention-allocation-quota";
            } else {
                return ::media::Status::failure(::media::ErrorInfo::unsupported(
                    "initial packet allocation budget has no prepared producer quota"));
            }
        } else if (producer.payloadKind == MediaPayloadKind::Frame) {
            if (node->kind == MediaNodeKind::HardwareTransfer &&
                node->options.value("transfer.direction") == "none") {
                authority = "reference-only-transfer-retains-original-producer-lease";
            } else {
                // Frame allocations cannot share the compressed-packet quota:
                // source frames may be pinned by every independent slow branch.
                auto quota = Arithmetic::multiply(retained, producer.maximumReservedBytes(),
                    "software frame reachable ownership upper bound");
                if (!quota) return ::media::Status::failure(quota.error());
                ownedBytes = quota.value();
                authority = "software-frame-reachable-retention-upper-bound";
            }
        } else return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "initial allocation budget has no typed payload semantics"));
        if (ownedBytes != 0 && ownedBytes < producer.maximumReservedBytes()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "producer allocation quota cannot admit its maximum atomic allocation"));
        }
        segment.entries.push_back(MediaFinalGraphResourceLedgerEntry{
            "producer-quota:" + node->name, {},
            producer.accounting == MediaGraphPayloadAllocationAccounting::
                ObservedOnlyExternalBytesAndEngineManagedObject
                ? MediaFinalGraphResourceScope::ObservedOnlyDeviceAndDriverAllocation
                : MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
            ownedBytes, 0, retained, retained, true, authority});
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::GraphBuild,
            "producer_quota node=" + node->name + " engine_bytes=" +
            std::to_string(ownedBytes) + " maximum_unit_bytes=" +
            std::to_string(producer.maximumReservationBytes) + " objects=" +
            std::to_string(retained) + " authority=" + authority);
        auto totalBytes = Arithmetic::add(bytes, ownedBytes, "partition allocation byte quotas");
        auto totalObjects = Arithmetic::add(objects, retained, "partition object credits");
        if (!totalBytes || !totalObjects) return ::media::Status::failure(
            !totalBytes ? totalBytes.error() : totalObjects.error());
        bytes = totalBytes.value();
        objects = totalObjects.value();
    }
    credits.maximumBytes = bytes;
    credits.maximumObjects = objects;
    credits.authority = "independent-producer-allocation-quotas+reachable-object-retention";
    if (!credits.isCompleteAndValid()) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("initial partition has no complete producer residence bound"));
    return ::media::Status::success();
}
} // namespace

::media::Result<MediaRealtimeInitialOutputResourcePartition>
MediaRealtimeInitialOutputResourcePartitionPlanner::plan(
    const MediaGraph& graph,
    const MediaRealtimeGraphResourceLedgerPlan& ledger,
    std::span<const MediaNodeId> outputNodes)
{
    using Result = ::media::Result<MediaRealtimeInitialOutputResourcePartition>;
    if (outputNodes.empty() || outputNodes.size() >= graph.nodeCount()) return Result::failure(
        ::media::ErrorInfo::invalidArgument("initial resource partition requires two nonempty execution segments"));
    for (std::size_t index = 0; index < outputNodes.size(); ++index) {
        if (!graph.findNode(outputNodes[index]) ||
            std::find(outputNodes.begin(), outputNodes.begin() + index, outputNodes[index]) !=
                outputNodes.begin() + index) return Result::failure(
            ::media::ErrorInfo::invalidArgument("initial resource partition contains invalid or duplicate nodes"));
    }
    std::vector<MediaNodeId> sharedNodes;
    for (const auto& node : graph.nodes()) {
        if (std::find(outputNodes.begin(), outputNodes.end(), node.id) == outputNodes.end()) {
            sharedNodes.push_back(node.id);
        }
    }
    auto complete = MediaFinalGraphResourceLedgerCompiler::compile(graph, ledger, {});
    if (!complete) return Result::failure(complete.error());
    auto shared = MediaFinalGraphResourceLedgerCompiler::compile(graph, ledger, sharedNodes);
    if (!shared) return Result::failure(shared.error());
    auto output = MediaFinalGraphResourceLedgerCompiler::compile(graph, ledger, outputNodes);
    if (!output) return Result::failure(output.error());
    for (auto* segment : {&shared.value(), &output.value()}) {
        if (auto status = boundProducerResidence(graph, complete.value(), ledger, *segment); !status) {
            return Result::failure(status.error());
        }
    }
    auto storage = Arithmetic::add(shared.value().admittedGraphPayloadAndReservedStorageBytes,
        output.value().admittedGraphPayloadAndReservedStorageBytes, "partition reserved storage");
    auto payload = Arithmetic::add(shared.value().payloadCreditPlan.maximumBytes,
        output.value().payloadCreditPlan.maximumBytes, "partition payload budget");
    if (!storage || !payload) return Result::failure(!storage ? storage.error() : payload.error());
    auto total = Arithmetic::add(storage.value(), payload.value(), "initial partition admission");
    if (!total) return Result::failure(total.error());
    // The earlier single-pipeline estimate is not an external deployment cap.
    // Publish the sum of independent allocation quotas and reserved storage.
    for (auto* segment : {&shared.value(), &output.value()}) {
        auto maximum = Arithmetic::add(segment->payloadCreditPlan.maximumBytes,
            segment->admittedGraphPayloadAndReservedStorageBytes,
            "execution segment allocation and storage envelope");
        if (!maximum) return Result::failure(maximum.error());
        segment->maximumGraphPayloadAndReservedStorageBytes = maximum.value();
    }
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::GraphBuild,
        "initial_partition engine_bytes=" + std::to_string(total.value()) +
        " shared_payload_bytes=" + std::to_string(shared.value().payloadCreditPlan.maximumBytes) +
        " output_payload_bytes=" + std::to_string(output.value().payloadCreditPlan.maximumBytes) +
        " reserved_storage_bytes=" + std::to_string(storage.value()));
    return Result::success({std::move(shared).value(), std::move(output).value()});
}

} // namespace media::ffmpeg::graph

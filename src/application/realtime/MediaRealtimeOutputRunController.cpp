#include "application/realtime/MediaRealtimeOutputRunController.h"

#include "application/realtime/MediaRealtimeOutputPreparer.h"
#include "application/realtime/MediaRealtimeEncodingGroupRegistry.h"
#include "application/realtime/MediaRealtimeSegmentFactory.h"
#include "internal/graph/nodes/video/EncodedVideoOutputFanoutNode.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/realtime/MediaDatagramServiceScopePlanner.h"
#include "internal/graph/runtime/network/MediaDatagramServiceScopeArbiter.h"
#include "internal/graph/planner/realtime/MediaRealtimeBranchDrainPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeInitialOutputResourcePartition.h"
#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/nodes/video/VideoOutputFanoutNode.h"
#include "internal/graph/nodes/output/MediaScheduledDatagramSenderNode.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <future>
#include <map>
#include <set>
#include <thread>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

struct OutputObserverFailure final { std::exception_ptr exception; };

MediaRealtimeOutputFailureStage outputFailureStage(MediaGraphWorkerFailurePhase phase)
{
    switch (phase) {
    case MediaGraphWorkerFailurePhase::Preparation: return MediaRealtimeOutputFailureStage::Preparation;
    case MediaGraphWorkerFailurePhase::Publication: return MediaRealtimeOutputFailureStage::Publication;
    case MediaGraphWorkerFailurePhase::Runtime: return MediaRealtimeOutputFailureStage::Runtime;
    case MediaGraphWorkerFailurePhase::Drain: return MediaRealtimeOutputFailureStage::Drain;
    case MediaGraphWorkerFailurePhase::Release: return MediaRealtimeOutputFailureStage::Release;
    }
    std::terminate();
}

MediaGraphWorkerFailure outputFailure(::media::ErrorInfo error, MediaRealtimeOutputFailureStage stage)
{
    MediaGraphWorkerFailurePhase phase;
    switch (stage) {
    case MediaRealtimeOutputFailureStage::Planning:
    case MediaRealtimeOutputFailureStage::Preparation: phase = MediaGraphWorkerFailurePhase::Preparation; break;
    case MediaRealtimeOutputFailureStage::Publication: phase = MediaGraphWorkerFailurePhase::Publication; break;
    case MediaRealtimeOutputFailureStage::Runtime: phase = MediaGraphWorkerFailurePhase::Runtime; break;
    case MediaRealtimeOutputFailureStage::Drain: phase = MediaGraphWorkerFailurePhase::Drain; break;
    case MediaRealtimeOutputFailureStage::Release: phase = MediaGraphWorkerFailurePhase::Release; break;
    default: std::terminate();
    }
    return {{}, MediaNodeKind::Unknown, "output lifecycle", std::move(error), phase};
}

struct EgressAccount final {
    std::uint64_t capacity;
    std::atomic<std::uint64_t> reserved{0};
};

class OutputReservation final : public MediaRuntimeBranchResourceReservation {
public:
    OutputReservation(std::shared_ptr<EgressAccount> account, std::uint64_t bytes,
        std::shared_ptr<MediaGraphPayloadBranchReservation> payload,
        std::shared_ptr<MediaGraphPayloadRetentionReservation> retention)
        : m_account(std::move(account)), m_bytes(bytes), m_payload(std::move(payload)),
          m_retention(std::move(retention)) {}
    ~OutputReservation() override { m_account->reserved.fetch_sub(m_bytes); }
private:
    std::shared_ptr<EgressAccount> m_account;
    std::uint64_t m_bytes;
    std::shared_ptr<MediaGraphPayloadBranchReservation> m_payload;
    std::shared_ptr<MediaGraphPayloadRetentionReservation> m_retention;
};

class EncodingReservation final : public MediaRuntimeBranchResourceReservation {
public:
    EncodingReservation(std::shared_ptr<MediaGraphPayloadBranchReservation> payload,
        std::shared_ptr<MediaGraphPayloadRetentionReservation> retention,
        std::shared_ptr<MediaGraphPayloadBranchReservation> fixedStorage)
        : m_payload(std::move(payload)), m_retention(std::move(retention)),
          m_fixedStorage(std::move(fixedStorage)) {}
private:
    std::shared_ptr<MediaGraphPayloadBranchReservation> m_payload;
    std::shared_ptr<MediaGraphPayloadRetentionReservation> m_retention;
    std::shared_ptr<MediaGraphPayloadBranchReservation> m_fixedStorage;
};

void addMetrics(MediaGraphRuntimeMetrics& target, const MediaGraphRuntimeMetrics& value)
{
    target.workerIterations += value.workerIterations;
    target.workerProcessCalls += value.workerProcessCalls;
    target.workerProgress += value.workerProgress;
    target.workerWaits += value.workerWaits;
    target.workerWakeups += value.workerWakeups;
    target.workerErrors += value.workerErrors;
    target.errorCount += value.errorCount;
    target.totalPushed += value.totalPushed;
    target.totalPopped += value.totalPopped;
    target.droppedBuffers += value.droppedBuffers;
    target.encodedPacketsPushed += value.encodedPacketsPushed;
    target.encodedPacketsPopped += value.encodedPacketsPopped;
    target.threadCount += value.threadCount;
    target.activeWorkers += value.activeWorkers;
    target.queuedBuffers += value.queuedBuffers;
    target.peakQueuedBuffers += value.peakQueuedBuffers;
}

} // namespace

class MediaRealtimeOutputRunController::Impl final {
public:
    struct Output final {
        std::uint64_t id;
        std::uint64_t groupId;
        std::shared_ptr<MediaRuntimeBranch> branch;
        std::vector<MediaNodeId> nodes;
        MediaEdgeId frame;
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority;
        MediaRunningTime startupWait;
        std::uint64_t generation;
        std::chrono::steady_clock::time_point startedAt;
        MediaRealtimeOutputSnapshot snapshot;
        std::chrono::steady_clock::time_point transactionDeadline = std::chrono::steady_clock::time_point::max();
    };
    struct Preparation final {
        std::uint64_t id;
        std::uint64_t graphVersion;
        std::uint64_t generation;
        MediaRealtimeVideoOutputRequest request;
        MediaBufferRef source;
        MediaBufferRef timestamp;
        ::media::ffmpeg::BufferRefPtr hardware;
        std::shared_ptr<const MediaGraph> graph;
        MediaDecoderRuntimeFacts decoderFacts;
        std::vector<MediaRealtimeExistingVideoEncodingGroup> groups;
        std::chrono::steady_clock::time_point deadline;
        bool expired = false;
    };

    Impl(const MediaRealtimeRtpTranscodeRequest& request, MediaRealtimeVideoSessionFacts plan,
        MediaRealtimeInitialOutputProducts initial, MediaRuntimeBranchDrainPlan drainPlan,
        MediaGraphRuntime& runtime, MediaRealtimeVideoRunControl& control,
        const MediaRealtimeVideoRunObserver& observer, const MediaRealtimeVideoRunPolicy& policy)
        : request(request), plan(std::move(plan)), initial(initial), drainPlan(drainPlan), runtime(runtime), control(control),
          observer(observer), policy(policy), graph(std::make_shared<const MediaGraph>(*runtime.graph()))
    {
        egress = std::make_shared<EgressAccount>();
        egress->capacity = initial.serviceScope.capacityWireBytesPerSecond;
    }

    ::media::Status publish(MediaRealtimeOutputSnapshot snapshot)
    {
        control.publishOutputSnapshot(snapshot);
        if (observer.outputChanged) {
            try { observer.outputChanged(snapshot); }
            catch (...) { throw OutputObserverFailure{std::current_exception()}; }
        }
        return ::media::Status::success();
    }

    ::media::Status reject(std::uint64_t id, const std::string& path,
        MediaRealtimeOutputFailureStage stage, ::media::ErrorInfo error)
    {
        publish({id, MediaRealtimeOutputState::Failed, stage, error.message, path, error});
        // The rejecting call may still own a prepared encoder on its stack.
        // Notify retirement only after the enclosing transaction has unwound.
        pendingTerminal = MediaRealtimeOutputSnapshot{
            id, MediaRealtimeOutputState::Retired, stage, error.message, path, error};
        if (changingGroup) {
            auto group = registry.groups().find(*changingGroup);
            if (group != registry.groups().end() && group->second.outputCount == 0) {
                fanout->unsubscribe(group->second.branch->id());
                group->second.draining = true;
                group->second.branch->fail(outputFailure(error, stage));
                pendingTerminalGroup = *changingGroup;
            }
            changingGroup.reset();
        }
        return ::media::Status::success();
    }

    void publishPendingTerminal()
    {
        if (!pendingTerminal) return;
        if (pendingTerminalGroup && registry.groups().contains(*pendingTerminalGroup)) return;
        pendingTerminalGroup.reset();
        auto terminal = std::move(*pendingTerminal);
        pendingTerminal.reset();
        control.completeOutputChange();
        if (changingOutput == terminal.outputId) changingOutput.reset();
        publish(std::move(terminal));
    }

    ::media::Result<std::shared_ptr<MediaRuntimeBranchResourceReservation>> reserveProtocol(
        std::uint64_t bytes,
        std::shared_ptr<MediaGraphPayloadBranchReservation> payloadLease,
        const MediaGraphPayloadRetentionGrowth* retentionGrowth)
    {
        using Result = ::media::Result<std::shared_ptr<MediaRuntimeBranchResourceReservation>>;
        const auto used = egress->reserved.load();
        if (!bytes || used > egress->capacity || bytes > egress->capacity - used)
            return Result::failure(::media::ErrorInfo::unsupported("aggregate output peak wire traffic exceeds session egress capacity"));
        std::shared_ptr<MediaGraphPayloadRetentionReservation> retentionLease;
        if (retentionGrowth) {
            auto ledger = runtime.context().payloadCreditLedger();
            if (!ledger) return Result::failure(::media::ErrorInfo::notInitialized("shared retention ledger is unavailable"));
            auto admitted = ledger->reserveRetentionGrowth(*retentionGrowth);
            if (!admitted) return Result::failure(admitted.error());
            retentionLease = std::move(admitted).value();
        }
        auto result = std::make_shared<OutputReservation>(egress, bytes, std::move(payloadLease), std::move(retentionLease));
        egress->reserved.fetch_add(bytes);
        return Result::success(std::move(result));
    }

    ::media::Result<MediaRuntimeSegmentOutputBinding> exportSource(MediaNodeId node, MediaPortId port)
    {
        for (auto& [id, group] : registry.groups()) {
            if (std::find(group.nodes.begin(), group.nodes.end(), node) != group.nodes.end())
                return group.branch->context().exportOutput(port);
        }
        return runtime.context().exportOutput(port);
    }

    ::media::Result<MediaEdgeId> incomingEdge(const MediaRuntimeBranch& branch,
        const MediaGraph& graph, const MediaEndpoint& endpoint) const
    {
        const auto* port = graph.findOutputPort(endpoint.node, endpoint.port);
        if (port) for (const auto id : branch.inputEdges()) {
            const auto* edge = graph.findEdge(id);
            if (edge && edge->from.nodeId == endpoint.node && edge->from.portId == port->id)
                return ::media::Result<MediaEdgeId>::success(id);
        }
        return ::media::Result<MediaEdgeId>::failure(
            ::media::ErrorInfo::notInitialized("segment lacks its planned media input"));
    }

    void unsubscribe(Output& output)
    {
        const auto group = registry.groups().find(output.groupId);
        if (group != registry.groups().end()) group->second.fanout->unsubscribe(output.branch->id());
    }

    ::media::Status initialize(std::uint64_t id)
    {
        auto topology = MediaRealtimeRtpTranscodeGraphBuilder::initialOutputTopology(*graph);
        if (!topology) return ::media::Status::failure(topology.error());
        fanoutId = topology.value().sourceFanout;
        fanout = dynamic_cast<VideoOutputFanoutNode*>(runtime.scheduler().findNode(fanoutId));
        if (!fanout) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial graph lacks its shared frame fanout"));
        for (const auto nodeId : topology.value().sharedNodeIds) {
            const auto* node = graph->findNode(nodeId);
            if (node && node->kind == MediaNodeKind::CodecResolver) {
                resolver = dynamic_cast<CodecResolverNode*>(runtime.scheduler().findNode(nodeId));
                resolverId = nodeId;
            }
        }
        if (!resolver) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial graph lacks its shared source resolver"));
        for (const auto& edge : graph->edges()) {
            const auto* target = graph->findPort(edge.to.portId);
            const auto* source = graph->findPort(edge.from.portId);
            if (edge.to.nodeId == resolverId && target && source && target->name == "format")
                formatSource = {edge.from.nodeId, source->name};
        }
        if (!formatSource.valid()) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial source format endpoint is missing"));
        for (const auto nodeId : topology.value().outputNodeIds) {
            auto* sender = dynamic_cast<MediaScheduledDatagramSenderNode*>(runtime.scheduler().findNode(nodeId));
            if (sender) serviceScopeArbiter = sender->serviceScopeArbiter();
        }
        if (!serviceScopeArbiter) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial sender has no shared interface arbiter"));
        auto ledger = runtime.context().payloadCreditLedger();
        if (!plan.resourceLedger || !ledger) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial resource ledger is missing"));
        auto partition = MediaRealtimeInitialOutputResourcePartitionPlanner::plan(*graph, *plan.resourceLedger, topology.value());
        if (!partition) return ::media::Status::failure(partition.error());
        auto encodingLease = ledger->extractInitialBranch(std::move(partition.value().shared.payloadCreditPlan),
            std::move(partition.value().encoding.payloadCreditPlan));
        if (!encodingLease) return ::media::Status::failure(encodingLease.error());
        auto sharedStorage = ledger->extractInitialFixedStorage(
            partition.value().shared.admittedGraphPayloadAndReservedStorageBytes);
        if (!sharedStorage) return ::media::Status::failure(sharedStorage.error());
        sharedStorageLease = std::move(sharedStorage).value();
        auto encodingStorage = ledger->extractInitialFixedStorage(
            partition.value().encoding.admittedGraphPayloadAndReservedStorageBytes);
        if (!encodingStorage) return ::media::Status::failure(encodingStorage.error());
        auto protocolLease = ledger->extractInitialFixedStorage(partition.value().protocolFixedStorageBytes);
        if (!protocolLease) return ::media::Status::failure(protocolLease.error());
        auto groupId = registry.allocateGroupId();
        auto groupSegmentId = registry.allocateSegmentId();
        auto outputSegmentId = registry.allocateSegmentId();
        if (!groupId || !groupSegmentId || !outputSegmentId) return ::media::Status::failure(
            !groupId ? groupId.error() : !groupSegmentId ? groupSegmentId.error() : outputSegmentId.error());
        auto resources = std::make_shared<EncodingReservation>(std::move(encodingLease).value(), nullptr,
            std::move(encodingStorage).value());
        const std::array retirementProducers{resolverId};
        std::vector<MediaRuntimeSegmentOutputBinding> upstream;
        const auto gather = [&](const std::vector<MediaNodeId>& nodes) -> ::media::Status {
            std::set<std::uint32_t> ports;
            upstream.clear();
            for (const auto& edge : graph->edges()) {
                if (std::find(nodes.begin(), nodes.end(), edge.to.nodeId) == nodes.end() ||
                    std::find(nodes.begin(), nodes.end(), edge.from.nodeId) != nodes.end() ||
                    !ports.insert(edge.from.portId.value).second) continue;
                auto exported = exportSource(edge.from.nodeId, edge.from.portId);
                if (!exported) return ::media::Status::failure(exported.error());
                upstream.push_back(std::move(exported).value());
            }
            return ::media::Status::success();
        };
        auto inputs = gather(topology.value().encodingNodeIds);
        if (!inputs) return inputs;
        auto encoding = runtime.extractInitialBranch(groupSegmentId.value(), topology.value().encodingNodeIds,
            std::move(resources), retirementProducers, upstream);
        if (!encoding) return ::media::Status::failure(encoding.error());
        auto* encodedFanout = dynamic_cast<EncodedVideoOutputFanoutNode*>(encoding.value()->findNode(topology.value().encoded.packet.node));
        if (!encodedFanout) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial encoding segment has no packet fanout"));
        auto join = MediaRealtimeVideoEncodingGroupContractPlanner::joinPlan(plan.videoPlan);
        if (!join) return ::media::Status::failure(join.error());
        auto bound = encodedFanout->bindJoinPlan(join.value());
        if (!bound) return bound;
        auto frame = incomingEdge(*encoding.value(), *graph, {fanoutId, "frame"});
        if (!frame) return ::media::Status::failure(frame.error());
        registry.groups().emplace(groupId.value(), MediaRealtimeEncodingGroup{groupId.value(), initial.initialGeneration,
            encoding.value(), topology.value().encodingNodeIds, frame.value(), topology.value().encoded,
            encodedFanout, nullptr, nullptr, 0, false, true});
        changingGroup = groupId.value();
        initialGroupId = groupId.value();
        inputs = gather(topology.value().outputNodeIds);
        if (!inputs) return inputs;
        auto outputLease = reserveProtocol(initial.peakWireBytesPerSecond, std::move(protocolLease).value(), nullptr);
        if (!outputLease) return ::media::Status::failure(outputLease.error());
        auto protocol = runtime.extractInitialBranch(outputSegmentId.value(), topology.value().outputNodeIds,
            std::move(outputLease).value(), {}, upstream);
        if (!protocol) return ::media::Status::failure(protocol.error());
        auto packet = incomingEdge(*protocol.value(), *graph, topology.value().encoded.packet);
        if (!packet) return ::media::Status::failure(packet.error());
        outputs.emplace(id, Output{id, groupId.value(), protocol.value(), topology.value().outputNodeIds, packet.value(),
            runtime.protocolOutputAuthority(), initial.maximumStartupWait, initial.initialGeneration, {},
            {id, MediaRealtimeOutputState::Preparing, MediaRealtimeOutputFailureStage::Preparation,
                {}, request.output.sdpPath, std::nullopt}});
        registry.groups().at(groupId.value()).outputCount = 1;
        changingGroup.reset();
        return ::media::Status::success();
    }

    ::media::Status startInitial()
    {
        auto& output = outputs.begin()->second;
        auto& group = registry.groups().at(output.groupId);
        auto started = group.branch->start();
        if (!started) return started;
        started = output.branch->start();
        if (!started) return started;
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Publication;
        auto subscribed = group.fanout->subscribe(output.branch, output.frame);
        if (!subscribed) { output.branch->fail(outputFailure(subscribed.error(), MediaRealtimeOutputFailureStage::Publication)); return subscribed; }
        output.startedAt = std::chrono::steady_clock::now();
        output.snapshot.state = MediaRealtimeOutputState::WaitingForRandomAccess;
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Runtime;
        publish(output.snapshot);
        control.setOutputChangesEnabled(true);
        return ::media::Status::success();
    }

    void refreshInitialWitness() try
    {
        if (!initialGroupId) return;
        auto group = registry.groups().find(*initialGroupId);
        if (group == registry.groups().end() || group->second.witness || group->second.draining || group->second.branch->failure()) {
            initialGroupId.reset();
            return;
        }
        auto readback = resolver->encoderReadback();
        auto metadata = resolver->encoderParametersSnapshot();
        auto generation = fanout->sourceGeneration();
        auto hardware = fanout->hardwareFrames();
        if (!readback || !metadata || !generation || !hardware) return;
        auto actual = MediaRealtimeVideoEncodingGroupContractPlanner::plan(plan.videoPlan, fanoutId,
            generation.value(), plan.sourceTimeBase, plan.preparedVideoSource.frameRate, readback.value());
        if (!actual) { group->second.branch->fail(outputFailure(actual.error(), MediaRealtimeOutputFailureStage::Preparation)); return; }
        auto join = MediaRealtimeVideoEncodingGroupContractPlanner::joinPlan(plan.videoPlan);
        if (!join) { group->second.branch->fail(outputFailure(join.error(), MediaRealtimeOutputFailureStage::Preparation)); return; }
        group->second.witness = std::make_shared<const MediaRealtimeVideoEncodingWitness>(
            MediaRealtimeVideoEncodingWitness{plan.videoPlan, std::move(actual).value(), std::move(hardware).value(), join.value()});
        group->second.codecParameters = std::move(metadata);
        const auto* codecPort = graph->findOutputPort(group->second.encoded.codec.node, group->second.encoded.codec.port);
        if (!codecPort) {
            group->second.branch->fail(outputFailure(::media::ErrorInfo::notInitialized("initial protocol codec endpoint is missing"), MediaRealtimeOutputFailureStage::Publication));
            return;
        }
        for (auto& [id, output] : outputs) {
            if (output.groupId != group->first) continue;
            for (const auto edgeId : output.branch->inputEdges()) {
                if (edgeId == output.frame) continue;
                const auto* edge = graph->findEdge(edgeId);
                if (!edge || edge->from.portId != codecPort->id ||
                    output.branch->tryPublish(edgeId, group->second.codecParameters).outcome != MediaQueuePushOutcome::Accepted) {
                    group->second.branch->fail(outputFailure(::media::ErrorInfo::internalError("initial protocol codec replay was not accepted"), MediaRealtimeOutputFailureStage::Publication));
                    return;
                }
            }
        }
        auto subscribed = fanout->subscribe(group->second.branch, group->second.frameInput);
        if (!subscribed) { group->second.branch->fail(outputFailure(subscribed.error(), MediaRealtimeOutputFailureStage::Publication)); return; }
        initialGroupId.reset();
    }
    catch (const std::exception& error) {
        if (initialGroupId) {
            auto group = registry.groups().find(*initialGroupId);
            if (group != registry.groups().end()) group->second.branch->fail(
                outputFailure(::media::ErrorInfo::internalError(std::string("initial encoding witness publication failed: ") + error.what()),
                    MediaRealtimeOutputFailureStage::Preparation));
        }
    }

    template<class Action>
    ::media::Status transact(std::uint64_t id, const std::string& path,
        MediaRealtimeOutputFailureStage stage, Action&& action)
    {
        try { return action(); }
        catch (const OutputObserverFailure& failure) {
            if (preparation && !preparationResult.valid()) preparation.reset();
            std::rethrow_exception(failure.exception);
        }
        catch (const std::bad_alloc&) {
            return transactionFailed(id, path, stage,
                ::media::ErrorInfo::allocationFailed("output transaction allocation failed"));
        }
        catch (const std::exception& error) {
            return transactionFailed(id, path, stage, ::media::ErrorInfo::internalError(error.what()));
        }
        catch (...) {
            return transactionFailed(id, path, stage,
                ::media::ErrorInfo::internalError("output transaction raised an unknown exception"));
        }
    }

    ::media::Status transactionFailed(std::uint64_t id, const std::string& path,
        MediaRealtimeOutputFailureStage stage, const ::media::ErrorInfo& error)
    {
        if (auto found = outputs.find(id); found != outputs.end()) {
            unsubscribe(found->second);
            found->second.snapshot.stage = stage;
            found->second.branch->fail(outputFailure(error, stage));
            return ::media::Status::success();
        }
        if (preparation && !preparationResult.valid()) preparation.reset();
        return reject(id, path, stage, error);
    }

    ::media::Status beginPreparation(MediaRealtimeOutputChange change)
    {
        auto source = resolver->inputSnapshot();
        auto timestamp = resolver->timestampSource();
        auto snapshot = std::dynamic_pointer_cast<FFmpegInputSnapshotBuffer>(source);
        if (!source) return reject(change.outputId, change.addition->output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation,
            ::media::ErrorInfo::notInitialized("shared codec resolver has not published its input snapshot"));
        if (!snapshot) return reject(change.outputId, change.addition->output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation,
            ::media::ErrorInfo::invalidArgument("shared codec resolver input snapshot has an unexpected buffer type"));
        if (!snapshot->inputStreamSnapshot(plan.videoPlan.sourceStreamIndex))
            return reject(change.outputId, change.addition->output.sdpPath, MediaRealtimeOutputFailureStage::Preparation,
                ::media::ErrorInfo::notInitialized("shared input snapshot does not contain selected video stream " +
                    std::to_string(plan.videoPlan.sourceStreamIndex)));
        if (!timestamp) return reject(change.outputId, change.addition->output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation,
            ::media::ErrorInfo::notInitialized("shared codec resolver has not published timestamp-source metadata"));
        auto decoderFacts = resolver->decoderRuntimeFacts();
        if (!decoderFacts) return reject(change.outputId, change.addition->output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation, decoderFacts.error());
        auto generation = fanout->sourceGeneration();
        if (!generation) return reject(change.outputId, change.addition->output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation, generation.error());
        auto hardware = fanout->hardwareFrames();
        if (!hardware) return reject(change.outputId, change.addition->output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation, hardware.error());
        preparation = std::make_shared<Preparation>(Preparation{change.outputId, graphVersion, generation.value(),
            std::move(*change.addition), std::move(source), std::move(timestamp), std::move(hardware).value(), graph, std::move(decoderFacts).value(), registry.available(),
            std::chrono::steady_clock::now() + policy.firstOutputTimeout()});
        auto task = preparation;
        publish({task->id, MediaRealtimeOutputState::Preparing, MediaRealtimeOutputFailureStage::Preparation,
            "preparing output and resolving encoding-group reuse", task->request.output.sdpPath, std::nullopt});
        try {
        preparationResult = std::async(std::launch::async, [this, task]() {
            try {
                const auto snapshot = std::dynamic_pointer_cast<FFmpegInputSnapshotBuffer>(task->source);
                return MediaRealtimeOutputPreparer::prepare({task->request, request, plan,
                    *snapshot->inputStreamSnapshot(plan.videoPlan.sourceStreamIndex), task->hardware.get(), *task->graph,
                    formatSource, {{fanoutId, "frame"}, {resolverId, "timestamp_source"}},
                    "output." + std::to_string(task->id), task->generation, task->decoderFacts, task->groups});
            } catch (const std::exception& error) {
                return ::media::Result<MediaPreparedRealtimeOutput>::failure(::media::ErrorInfo::internalError(error.what()));
            }
        });
        } catch (const std::exception& error) {
            preparation.reset();
            return reject(task->id, task->request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation,
                ::media::ErrorInfo::internalError(error.what()));
        }
        return ::media::Status::success();
    }

    ::media::Status publishPrepared(MediaPreparedRealtimeOutput prepared, const Preparation& task)
    {
        auto generation = fanout->sourceGeneration();
        auto hardware = fanout->hardwareFrames();
        if (task.expired || std::chrono::steady_clock::now() >= task.deadline)
            return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation,
                ::media::ErrorInfo::notInitialized("output startup transaction exceeded the session first-output budget"));
        if (finishing) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Publication, ::media::ErrorInfo::cancelled("session is finishing before output publication"));
        if (!generation) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Publication, generation.error());
        if (!hardware) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Publication, hardware.error());
        if (generation.value() != task.generation) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Publication, ::media::ErrorInfo::cancelled("source generation changed before output publication"));
        if (graphVersion != task.graphVersion) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Publication, ::media::ErrorInfo::cancelled("logical graph version changed before output publication"));
        if ((hardware.value() ? hardware.value()->data : nullptr) != (task.hardware ? task.hardware->data : nullptr))
            return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication,
                ::media::ErrorInfo::cancelled("source hardware frame context changed before output publication"));
        const auto& video = std::get<MediaRealtimeVideoRuntimePlan>(prepared.plan.runtime);
        auto service = MediaDatagramServiceScopePlanner::plan(video.datagramTransport);
        if (!service || service.value() != initial.serviceScope) return reject(
            task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Planning,
            service ? ::media::ErrorInfo::unsupported("Output changes its prepared interface service contract")
                    : service.error());
        const auto publishedGraph = prepared.output.graph;
        if (!publishedGraph) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation, ::media::ErrorInfo::notInitialized("prepared protocol graph is missing"));
        auto parentAuthority = std::dynamic_pointer_cast<MediaVideoProtocolOutputRuntimeAuthority>(runtime.protocolOutputAuthority());
        if (!parentAuthority) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication,
            ::media::ErrorInfo::notInitialized("shared video protocol clock is unavailable"));
        auto authority = parentAuthority->fork(video.sessionKey, task.generation);
        if (!authority) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication, authority.error());
        auto ledger = runtime.context().payloadCreditLedger();
        if (!ledger) return reject(task.id, task.request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation, ::media::ErrorInfo::notInitialized("session resource ledger is missing"));
        const auto exporter = [this](MediaNodeId node, MediaPortId port) { return exportSource(node, port); };
        std::uint64_t groupId;
        const bool newGroup = std::holds_alternative<MediaRealtimeNewEncodingGroup>(prepared.encoding);
        if (newGroup) {
            auto& candidate = std::get<MediaRealtimeNewEncodingGroup>(prepared.encoding);
            if (!candidate.witness) return reject(task.id, task.request.output.sdpPath,
                MediaRealtimeOutputFailureStage::Preparation, ::media::ErrorInfo::notInitialized("encoding witness is missing"));
            auto allocatedGroup = registry.allocateGroupId();
            auto allocatedSegment = registry.allocateSegmentId();
            if (!allocatedGroup || !allocatedSegment) return reject(task.id, task.request.output.sdpPath,
                MediaRealtimeOutputFailureStage::Preparation, !allocatedGroup ? allocatedGroup.error() : allocatedSegment.error());
            groupId = allocatedGroup.value();
            auto payload = ledger->reserveBranch(candidate.segment.resources.payloadCreditPlan);
            if (!payload) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Planning, payload.error());
            auto retention = ledger->reserveRetentionGrowth(candidate.segment.sourceRetentionGrowth);
            if (!retention) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Planning, retention.error());
            auto fixedStorage = ledger->reserveFixedStorage(candidate.segment.resources.admittedGraphPayloadAndReservedStorageBytes);
            if (!fixedStorage) return reject(task.id, task.request.output.sdpPath,
                MediaRealtimeOutputFailureStage::Preparation, fixedStorage.error());
            auto resources = std::make_shared<EncodingReservation>(std::move(payload).value(), std::move(retention).value(),
                std::move(fixedStorage).value());
            auto created = MediaRealtimeSegmentFactory::create(allocatedSegment.value(), publishedGraph,
                candidate.segment.nodeIds, candidate.segment.threading, std::move(resources), nullptr, nullptr,
                candidate.encoder, runtime.context(), exporter);
            if (!created) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, created.error());
            if (!created.value().resolver || !created.value().encodedFanout) return reject(task.id, task.request.output.sdpPath,
                MediaRealtimeOutputFailureStage::Preparation, ::media::ErrorInfo::notInitialized("encoding segment lacks resolver or packet fanout"));
            auto metadata = created.value().resolver->encoderParametersSnapshot();
            if (!metadata) return reject(task.id, task.request.output.sdpPath,
                MediaRealtimeOutputFailureStage::Preparation, ::media::ErrorInfo::notInitialized("prepared encoder has no immutable codec parameters"));
            auto joined = created.value().encodedFanout->bindJoinPlan(candidate.witness->joinPlan);
            if (!joined) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, joined.error());
            auto frame = incomingEdge(*created.value().branch, *publishedGraph, {fanoutId, "frame"});
            if (!frame) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, frame.error());
            registry.groups().emplace(groupId, MediaRealtimeEncodingGroup{groupId, task.generation,
                created.value().branch, candidate.segment.nodeIds, frame.value(), candidate.segment.encoded,
                created.value().encodedFanout, candidate.witness, std::move(metadata)});
            changingGroup = groupId;
        } else {
            groupId = std::get<MediaRealtimeExistingEncodingGroup>(prepared.encoding).groupId;
            auto group = registry.groups().find(groupId);
            if (group == registry.groups().end() || group->second.draining || group->second.branch->failure() ||
                group->second.generation != task.generation || !group->second.codecParameters ||
                group->second.branch->state() != MediaRuntimeBranchState::Running)
                return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication,
                    ::media::ErrorInfo::cancelled("selected encoding group is no longer available"));
        }
        auto& group = registry.groups().at(groupId);
        auto storage = ledger->reserveFixedStorage(prepared.output.fixedStorageBytes);
        if (!storage) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Planning, storage.error());
        auto lease = reserveProtocol(video.datagramTransport.encode().wireTraffic.peakWireBytesPerSecond,
            std::move(storage).value(), &prepared.output.encodedRetentionGrowth);
        if (!lease) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Planning, lease.error());
        auto allocatedSegment = registry.allocateSegmentId();
        if (!allocatedSegment) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, allocatedSegment.error());
        auto created = MediaRealtimeSegmentFactory::create(allocatedSegment.value(), publishedGraph,
            prepared.output.nodeIds, prepared.output.threading, std::move(lease).value(), authority.value(),
            serviceScopeArbiter, nullptr, runtime.context(), exporter);
        if (!created) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, created.error());
        auto packet = incomingEdge(*created.value().branch, *publishedGraph, group.encoded.packet);
        if (!packet) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, packet.error());
        auto inserted = outputs.emplace(task.id, Output{task.id, groupId, created.value().branch,
            prepared.output.nodeIds, packet.value(), authority.value(), video.startup.maximumWait,
            task.generation, std::chrono::steady_clock::now(),
            {task.id, MediaRealtimeOutputState::WaitingForRandomAccess, MediaRealtimeOutputFailureStage::Preparation,
                {}, task.request.output.sdpPath, std::nullopt}});
        ++group.outputCount;
        changingGroup.reset();
        auto& output = inserted.first->second;
        output.transactionDeadline = task.deadline;
        if (newGroup) {
            auto status = group.branch->start();
            if (!status) { group.branch->fail(status.error()); return ::media::Status::success(); }
            for (const auto edgeId : group.branch->inputEdges()) {
                if (edgeId == group.frameInput) continue;
                const auto* edge = publishedGraph->findEdge(edgeId);
                MediaBufferRef metadata;
                if (edge->from.nodeId == formatSource.node) metadata = task.source;
                else if (edge->from.nodeId == resolverId) metadata = task.timestamp;
                if (!metadata || group.branch->tryPublish(edgeId, metadata).outcome != MediaQueuePushOutcome::Accepted) {
                    group.branch->fail(outputFailure(::media::ErrorInfo::internalError("encoding group metadata replay was not accepted"), MediaRealtimeOutputFailureStage::Publication));
                    return ::media::Status::success();
                }
            }
        }
        auto started = output.branch->start();
        if (!started) { output.branch->fail(started.error()); return ::media::Status::success(); }
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Publication;
        for (const auto edgeId : output.branch->inputEdges()) {
            if (edgeId == output.frame) continue;
            const auto* edge = publishedGraph->findEdge(edgeId);
            const auto* codecPort = publishedGraph->findOutputPort(group.encoded.codec.node, group.encoded.codec.port);
            if (!codecPort || edge->from.portId != codecPort->id ||
                output.branch->tryPublish(edgeId, group.codecParameters).outcome != MediaQueuePushOutcome::Accepted) {
                output.branch->fail(outputFailure(::media::ErrorInfo::internalError("protocol codec parameter replay was not accepted"), MediaRealtimeOutputFailureStage::Publication));
                return ::media::Status::success();
            }
        }
        auto subscribed = group.fanout->subscribe(output.branch, output.frame);
        if (!subscribed) { output.branch->fail(outputFailure(subscribed.error(), MediaRealtimeOutputFailureStage::Publication)); return ::media::Status::success(); }
        if (newGroup) {
            auto status = fanout->subscribe(group.branch, group.frameInput);
            if (!status) { group.branch->fail(outputFailure(status.error(), MediaRealtimeOutputFailureStage::Publication)); return ::media::Status::success(); }
        }
        graph = publishedGraph;
        group.graphPublished = true;
        ++graphVersion;
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeLifecycle,
            "output_encoding_group output_id=" + std::to_string(output.id) +
            " group_id=" + std::to_string(group.groupId) +
            " encoding_segment_id=" + std::to_string(group.branch->id()) +
            " protocol_segment_id=" + std::to_string(output.branch->id()) +
            " action=" + (newGroup ? "created" : "reused"));
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Runtime;
        publish(output.snapshot);
        // Keep the change busy until the branch is ready or has physically retired.
        return ::media::Status::success();
    }

    ::media::Status drain(Output& output)
    {
        unsubscribe(output);
        auto generation = fanout->sourceGeneration();
        if (generation) output.generation = generation.value();
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Drain;
        auto status = output.branch->beginDrain(output.generation, drainPlan);
        if (!status) output.branch->fail(outputFailure(status.error(), MediaRealtimeOutputFailureStage::Drain));
        else { output.snapshot.state = MediaRealtimeOutputState::Draining; output.snapshot.stage = MediaRealtimeOutputFailureStage::Drain; publish(output.snapshot); }
        return ::media::Status::success();
    }

    void pollGroups()
    {
        for (auto it = registry.groups().begin(); it != registry.groups().end();) {
            auto& group = it->second;
            if (const auto failure = group.branch->failure()) {
                fanout->unsubscribe(group.branch->id());
                for (auto& [outputId, output] : outputs) {
                    if (output.groupId != group.groupId) continue;
                    unsubscribe(output);
                    if (!output.branch->failure()) {
                        output.branch->fail(*failure);
                    }
                }
            }
            if (group.outputCount != 0) { ++it; continue; }
            if (!group.draining) {
                fanout->unsubscribe(group.branch->id());
                group.draining = true;
                if (group.branch->state() == MediaRuntimeBranchState::Running) {
                    auto status = group.branch->beginDrain(group.generation, drainPlan);
                    if (!status) group.branch->fail(outputFailure(status.error(), MediaRealtimeOutputFailureStage::Drain));
                } else if (group.branch->state() == MediaRuntimeBranchState::Prepared) {
                    group.branch->fail(::media::ErrorInfo::cancelled("unpublished encoding group has no consumers"));
                }
            }
            auto retired = group.branch->poll();
            if (!retired) group.branch->fail(retired.error());
            if (!retired || !retired.value()) { ++it; continue; }
            if (const auto failure = group.branch->failure(); failure && finishing && !finishFailure)
                finishFailure = failure->error;
            if (pendingTerminalGroup == group.groupId && pendingTerminal && !pendingTerminal->error && group.branch->failure()) {
                const auto failure = group.branch->failure();
                pendingTerminal->error = failure->error;
                pendingTerminal->detail = failure->error.message;
                pendingTerminal->stage = outputFailureStage(failure->phase);
                auto failed = *pendingTerminal;
                failed.state = MediaRealtimeOutputState::Failed;
                publish(std::move(failed));
            }
            addMetrics(retiredMetrics, group.branch->metrics());
            if (group.graphPublished) {
                auto nextGraph = std::make_shared<MediaGraph>(*graph);
                nextGraph->removeNodes(group.nodes);
                graph = std::move(nextGraph);
                ++graphVersion;
            }
            it = registry.groups().erase(it);
        }
    }

    ::media::Status poll()
    {
        refreshInitialWitness();
        // The application startup budget bounds the transaction logically; an
        // expired driver operation remains owned until it actually returns.
        if (preparation && !preparation->expired && std::chrono::steady_clock::now() >= preparation->deadline) {
            preparation->expired = true;
            const auto error = ::media::ErrorInfo::notInitialized(
                "output preparation exceeded the session first-output budget; driver work remains owned");
            publish({preparation->id, MediaRealtimeOutputState::Failed, MediaRealtimeOutputFailureStage::Preparation,
                error.message, preparation->request.output.sdpPath, error});
        }
        if (preparation && preparationResult.valid() &&
            preparationResult.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready) {
            auto task = std::move(preparation);
            transact(task->id, task->request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication, [&] {
                auto result = preparationResult.get();
                if (!result) return reject(task->id, task->request.output.sdpPath,
                    MediaRealtimeOutputFailureStage::Preparation, result.error());
                return publishPrepared(std::move(result).value(), *task);
            });
        }
        pollGroups();
        publishPendingTerminal();
        for (auto it = outputs.begin(); it != outputs.end();) {
            auto& output = it->second;
            if (output.snapshot.state == MediaRealtimeOutputState::WaitingForRandomAccess &&
                !output.branch->failure() &&
                std::chrono::steady_clock::now() < output.transactionDeadline &&
                std::chrono::steady_clock::now() - output.startedAt < std::chrono::nanoseconds(output.startupWait.nanoseconds()) &&
                output.authority->currentActivation() && output.branch->videoReadyEvidence() &&
                output.branch->videoReadyEvidence()->generation == output.generation) {
                output.snapshot.state = MediaRealtimeOutputState::Running;
                output.snapshot.stage = MediaRealtimeOutputFailureStage::Runtime;
                if (changingOutput == output.id) { control.completeOutputChange(); changingOutput.reset(); }
                publish(output.snapshot);
            }
            if (output.snapshot.state == MediaRealtimeOutputState::WaitingForRandomAccess &&
                (std::chrono::steady_clock::now() >= output.transactionDeadline ||
                    std::chrono::steady_clock::now() - output.startedAt >= std::chrono::nanoseconds(output.startupWait.nanoseconds())))
                output.branch->fail(::media::ErrorInfo::notInitialized("output produced no independently decodable media before its planned startup deadline"));
            if (output.branch->failure()) unsubscribe(output);
            auto retired = output.branch->poll();
            if (!retired) output.branch->fail(retired.error());
            if (const auto failure = output.branch->failure(); failure && output.snapshot.state != MediaRealtimeOutputState::Failed) {
                unsubscribe(output);
                output.snapshot.state = MediaRealtimeOutputState::Failed;
                output.snapshot.detail = failure->error.message;
                output.snapshot.error = failure->error;
                output.snapshot.stage = outputFailureStage(failure->phase);
                if (finishing && !finishFailure) finishFailure = failure->error;
                publish(output.snapshot);
            }
            if (retired && retired.value()) {
                unsubscribe(output);
                addMetrics(retiredMetrics, output.branch->metrics());
                auto nextGraph = std::make_shared<MediaGraph>(*graph);
                nextGraph->removeNodes(output.nodes);
                graph = std::move(nextGraph);
                ++graphVersion;
                output.snapshot.state = MediaRealtimeOutputState::Retired;
                auto terminal = std::move(output.snapshot);
                const auto groupId = output.groupId;
                it = outputs.erase(it);
                auto group = registry.groups().find(groupId);
                if (group != registry.groups().end()) --group->second.outputCount;
                if (changingOutput == terminal.outputId && group != registry.groups().end() && group->second.outputCount == 0) {
                    pendingTerminal = std::move(terminal);
                    pendingTerminalGroup = groupId;
                } else {
                    if (changingOutput == terminal.outputId) { control.completeOutputChange(); changingOutput.reset(); }
                    publish(std::move(terminal));
                }
            } else ++it;
        }
        pollGroups();
        publishPendingTerminal();
        if (!finishing && !preparation && !pendingTerminal) {
            if (auto change = control.takeOutputChange()) {
                changingOutput = change->outputId;
                if (change->addition) {
                    auto status = transact(change->outputId, change->addition->output.sdpPath,
                        MediaRealtimeOutputFailureStage::Preparation,
                        [&] { return beginPreparation(std::move(*change)); });
                    publishPendingTerminal();
                    return status;
                }
                auto found = outputs.find(change->outputId);
                if (found == outputs.end()) {
                    auto status = reject(change->outputId, {}, MediaRealtimeOutputFailureStage::Drain,
                        ::media::ErrorInfo::invalidArgument("output no longer has a running branch"));
                    publishPendingTerminal();
                    return status;
                }
                return drain(found->second);
            }
        }
        return ::media::Status::success();
    }

    ::media::Status finish(const ::media::Status& cause)
    {
        if (finished) return ::media::Status::success();
        finishing = true;
        control.setOutputChangesEnabled(false);
        if (!cause && (runtime.state() == MediaGraphRuntimeState::Compiled ||
            runtime.state() == MediaGraphRuntimeState::DefaultRegistrationPending))
            runtime.abort();
        else if (runtime.threadedRunning())
            runtime.threadedExecutor().requestStop(runtime.context());
        if (auto change = control.takeOutputChange()) reject(change->outputId,
            change->addition ? change->addition->output.sdpPath : std::string{}, MediaRealtimeOutputFailureStage::Publication,
            ::media::ErrorInfo::cancelled("session is finishing"));
        publishPendingTerminal();
        for (auto& [id, output] : outputs) {
            unsubscribe(output);
            if (!cause) output.branch->fail(cause.error());
            else if (output.branch->state() == MediaRuntimeBranchState::Running) drain(output);
        }
        while (preparation || !outputs.empty() || !registry.groups().empty()) {
            poll();
            if (preparation || !outputs.empty() || !registry.groups().empty()) std::this_thread::sleep_for(policy.pollInterval());
        }
        control.completeOutputChange();
        finished = true;
        return finishFailure ? ::media::Status::failure(*finishFailure) : ::media::Status::success();
    }

    const MediaRealtimeRtpTranscodeRequest request;
    const MediaRealtimeVideoSessionFacts plan;
    const MediaRealtimeInitialOutputProducts initial;
    const MediaRuntimeBranchDrainPlan drainPlan;
    MediaGraphRuntime& runtime;
    MediaRealtimeVideoRunControl& control;
    const MediaRealtimeVideoRunObserver& observer;
    const MediaRealtimeVideoRunPolicy& policy;
    std::shared_ptr<const MediaGraph> graph;
    std::shared_ptr<EgressAccount> egress;
    std::shared_ptr<MediaGraphPayloadBranchReservation> sharedStorageLease;
    // Retained through zero outputs; joining a branch never resets virtual time.
    std::shared_ptr<MediaDatagramServiceScopeArbiter> serviceScopeArbiter;
    VideoOutputFanoutNode* fanout = nullptr;
    CodecResolverNode* resolver = nullptr;
    MediaNodeId fanoutId;
    MediaNodeId resolverId;
    MediaEndpoint formatSource;
    std::map<std::uint64_t, Output> outputs;
    MediaRealtimeEncodingGroupRegistry registry;
    std::optional<std::uint64_t> initialGroupId;
    std::shared_ptr<Preparation> preparation;
    std::future<::media::Result<MediaPreparedRealtimeOutput>> preparationResult;
    std::optional<std::uint64_t> changingOutput;
    std::optional<MediaRealtimeOutputSnapshot> pendingTerminal;
    std::optional<std::uint64_t> pendingTerminalGroup;
    std::optional<std::uint64_t> changingGroup;
    MediaGraphRuntimeMetrics retiredMetrics;
    std::uint64_t graphVersion = 0;
    std::optional<::media::ErrorInfo> finishFailure;
    bool finishing = false;
    bool finished = false;
};

MediaRealtimeOutputRunController::MediaRealtimeOutputRunController(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl)) {}

MediaRealtimeOutputRunController::~MediaRealtimeOutputRunController()
{
    if (!m_impl->finished) {
        try { m_impl->finish(::media::Status::failure(::media::ErrorInfo::cancelled("output controller is being released"))); }
        catch (...) { /* Owned futures and branches still join before destruction. */ }
    }
}

::media::Result<std::unique_ptr<MediaRealtimeOutputRunController>>
MediaRealtimeOutputRunController::create(const MediaRealtimeRtpTranscodeRequest& request,
    MediaRealtimeVideoSessionFacts plan, MediaRealtimeInitialOutputProducts initial,
    MediaGraphRuntime& runtime,
    MediaRealtimeVideoRunControl& control, const MediaRealtimeVideoRunObserver& observer,
    const MediaRealtimeVideoRunPolicy& policy, std::uint64_t initialOutputId)
{
    using Result = ::media::Result<std::unique_ptr<MediaRealtimeOutputRunController>>;
    auto silence = MediaRunningTime::checkedFromTicks(policy.progressTimeout().count(),
        std::chrono::milliseconds::period::num, std::chrono::milliseconds::period::den);
    if (!silence) return Result::failure(silence.error());
    auto drainPlan = MediaRealtimeBranchDrainPlanner::plan(silence.value());
    if (!drainPlan) return Result::failure(drainPlan.error());
    auto impl = std::make_unique<Impl>(request, std::move(plan), initial, drainPlan.value(), runtime, control, observer, policy);
    auto initialized = impl->initialize(initialOutputId);
    if (!initialized) {
        impl->reject(initialOutputId, request.output.sdpPath,
            MediaRealtimeOutputFailureStage::Preparation, initialized.error());
        impl->finish(::media::Status::failure(initialized.error()));
        return Result::failure(initialized.error());
    }
    return Result::success(std::unique_ptr<MediaRealtimeOutputRunController>(new MediaRealtimeOutputRunController(std::move(impl))));
}

::media::Status MediaRealtimeOutputRunController::startInitial() { return m_impl->startInitial(); }
::media::Status MediaRealtimeOutputRunController::poll() { return m_impl->poll(); }
bool MediaRealtimeOutputRunController::hasOutputs() const noexcept { return !m_impl->outputs.empty(); }
::media::Status MediaRealtimeOutputRunController::finish(const ::media::Status& cause) { return m_impl->finish(cause); }
void MediaRealtimeOutputRunController::aggregate(MediaGraphRuntimeReport& report) const
{
    addMetrics(report.metrics, m_impl->retiredMetrics);
    for (const auto& [id, output] : m_impl->outputs) addMetrics(report.metrics, output.branch->metrics());
    for (const auto& [id, group] : m_impl->registry.groups()) addMetrics(report.metrics, group.branch->metrics());
}

} // namespace media::ffmpeg::graph

#include "application/realtime/MediaRealtimeOutputRunController.h"

#include "application/realtime/MediaRealtimeOutputPreparer.h"
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
        return ::media::Status::success();
    }

    void publishPendingTerminal()
    {
        if (!pendingTerminal) return;
        auto terminal = std::move(*pendingTerminal);
        pendingTerminal.reset();
        control.completeOutputChange();
        if (changingOutput == terminal.outputId) changingOutput.reset();
        publish(std::move(terminal));
    }

    ::media::Result<std::shared_ptr<MediaRuntimeBranchResourceReservation>> reserve(
        std::uint64_t bytes, const MediaGraphPayloadCreditPlan* payload,
        std::shared_ptr<MediaGraphPayloadBranchReservation> payloadLease,
        const MediaGraphPayloadRetentionGrowth* retentionGrowth)
    {
        using Result = ::media::Result<std::shared_ptr<MediaRuntimeBranchResourceReservation>>;
        const auto used = egress->reserved.load();
        if (!bytes || used > egress->capacity || bytes > egress->capacity - used)
            return Result::failure(::media::ErrorInfo::unsupported("aggregate output peak wire traffic exceeds session egress capacity"));
        if (payload) {
            auto ledger = runtime.context().payloadCreditLedger();
            if (!ledger) return Result::failure(::media::ErrorInfo::notInitialized("session payload ledger is unavailable"));
            auto admitted = ledger->reserveBranch(*payload);
            if (!admitted) return Result::failure(admitted.error());
            payloadLease = std::move(admitted).value();
        }
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

    ::media::Status initialize(std::uint64_t id)
    {
        for (const auto& node : graph->nodes()) {
            if (node.kind == MediaNodeKind::VideoOutputFanout) {
                if (fanout) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("video session has multiple decoded-frame fanouts"));
                fanout = dynamic_cast<VideoOutputFanoutNode*>(runtime.scheduler().findNode(node.id));
                fanoutId = node.id;
            }
        }
        if (!fanout) return ::media::Status::failure(::media::ErrorInfo::notInitialized("video session has no shared decoded-frame fanout"));
        std::set<std::uint32_t> shared{fanoutId.value};
        bool changed;
        do {
            changed = false;
            for (const auto& edge : graph->edges()) {
                if (shared.contains(edge.to.nodeId.value))
                    changed = shared.insert(edge.from.nodeId.value).second || changed;
            }
        } while (changed);
        std::vector<MediaNodeId> outputNodes;
        MediaScheduledDatagramSenderNode* sender = nullptr;
        for (const auto& node : graph->nodes()) {
            if (!shared.contains(node.id.value)) {
                outputNodes.push_back(node.id);
                if (node.kind == MediaNodeKind::ScheduledDatagramSender)
                    sender = dynamic_cast<MediaScheduledDatagramSenderNode*>(runtime.scheduler().findNode(node.id));
            } else if (node.kind == MediaNodeKind::CodecResolver) {
                resolver = dynamic_cast<CodecResolverNode*>(runtime.scheduler().findNode(node.id));
                resolverId = node.id;
            }
        }
        if (!resolver || !sender) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial output lacks source resolver or datagram sender"));
        serviceScopeArbiter = sender->serviceScopeArbiter();
        if (!serviceScopeArbiter) return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("Initial sender lacks its registered shared service scope"));
        for (const auto& edge : graph->edges()) {
            if (edge.to.nodeId == resolverId) {
                const auto* target = graph->findPort(edge.to.portId);
                const auto* source = graph->findPort(edge.from.portId);
                if (target && source && target->name == "format") formatSource = {edge.from.nodeId, source->name};
            }
        }
        if (!formatSource.node.isValid() || !graph->findOutputPort(resolverId, "timestamp_source"))
            return ::media::Status::failure(::media::ErrorInfo::notInitialized("shared video source metadata endpoints are unavailable"));
        if (!plan.resourceLedger || !runtime.context().payloadCreditLedger())
            return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial output resource ledger is unavailable"));
        auto partition = MediaRealtimeInitialOutputResourcePartitionPlanner::plan(*graph, *plan.resourceLedger, outputNodes);
        if (!partition) return ::media::Status::failure(partition.error());
        auto payloadLease = runtime.context().payloadCreditLedger()->extractInitialBranch(
            std::move(partition.value().shared.payloadCreditPlan), std::move(partition.value().output.payloadCreditPlan));
        if (!payloadLease) return ::media::Status::failure(payloadLease.error());
        auto lease = reserve(initial.peakWireBytesPerSecond, nullptr, std::move(payloadLease).value(), nullptr);
        if (!lease) return ::media::Status::failure(lease.error());
        const std::array retirementProducers{resolverId};
        auto branch = runtime.extractInitialBranch(id, outputNodes, std::move(lease).value(), retirementProducers);
        if (!branch) return ::media::Status::failure(branch.error());
        MediaEdgeId frame;
        for (const auto edgeId : branch.value()->inputEdges()) {
            const auto* edge = graph->findEdge(edgeId);
            if (edge && edge->from.nodeId == fanoutId) frame = edgeId;
        }
        if (!frame.isValid()) return ::media::Status::failure(::media::ErrorInfo::notInitialized("initial output has no decoded-frame input"));
        outputs.emplace(id, Output{id, std::move(branch).value(), std::move(outputNodes), frame,
            runtime.protocolOutputAuthority(),
            initial.maximumStartupWait, initial.initialGeneration, {},
            {id, MediaRealtimeOutputState::Preparing, MediaRealtimeOutputFailureStage::Preparation,
                {}, request.output.sdpPath, std::nullopt}});
        return ::media::Status::success();
    }

    ::media::Status startInitial()
    {
        auto& output = outputs.begin()->second;
        auto started = output.branch->start();
        if (!started) return started;
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Publication;
        auto subscribed = fanout->subscribe(output.branch, output.frame);
        if (!subscribed) { output.branch->fail(subscribed.error()); return subscribed; }
        output.startedAt = std::chrono::steady_clock::now();
        output.snapshot.state = MediaRealtimeOutputState::WaitingForRandomAccess;
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Runtime;
        publish(output.snapshot);
        control.setOutputChangesEnabled(true);
        return ::media::Status::success();
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
            fanout->unsubscribe(id);
            found->second.snapshot.stage = stage;
            found->second.branch->fail(error);
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
            std::move(*change.addition), std::move(source), std::move(timestamp), std::move(hardware).value(), graph, std::move(decoderFacts).value(),
            std::chrono::steady_clock::now() + policy.firstOutputTimeout()});
        auto task = preparation;
        publish({task->id, MediaRealtimeOutputState::Preparing, MediaRealtimeOutputFailureStage::Preparation,
            "preparing an owned output encoder", task->request.output.sdpPath, std::nullopt});
        try {
        preparationResult = std::async(std::launch::async, [this, task]() {
            try {
                const auto snapshot = std::dynamic_pointer_cast<FFmpegInputSnapshotBuffer>(task->source);
                return MediaRealtimeOutputPreparer::prepare({task->request, request, plan,
                    *snapshot->inputStreamSnapshot(plan.videoPlan.sourceStreamIndex), task->hardware.get(), *task->graph,
                    formatSource, {{fanoutId, "frame"}, {resolverId, "timestamp_source"}},
                    "output." + std::to_string(task->id), task->generation, task->decoderFacts});
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
        auto lease = reserve(video.datagramTransport.encode().wireTraffic.peakWireBytesPerSecond,
            &prepared.branch.resources.payloadCreditPlan, nullptr, &prepared.branch.sourceRetentionGrowth);
        if (!lease) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Planning, lease.error());
        auto parentAuthority = std::dynamic_pointer_cast<MediaVideoProtocolOutputRuntimeAuthority>(runtime.protocolOutputAuthority());
        if (!parentAuthority) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication,
            ::media::ErrorInfo::notInitialized("shared video protocol clock is unavailable"));
        auto authority = parentAuthority->fork(video.sessionKey, task.generation);
        if (!authority) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Publication, authority.error());
        auto publishedGraph = std::make_shared<const MediaGraph>(std::move(prepared.branch.graph));
        std::vector<std::unique_ptr<MediaRuntimeNode>> nodes;
        MediaScheduledDatagramSenderNode* sender = nullptr;
        for (const auto id : prepared.branch.nodeIds) {
            const auto* node = publishedGraph->findNode(id);
            if (!node) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation,
                ::media::ErrorInfo::internalError("prepared output node disappeared"));
            auto created = node->kind == MediaNodeKind::VideoOutputScheduler
                ? MediaRuntimeNodeFactory::createVideoOutputScheduler(*node, authority.value())
                : node->kind == MediaNodeKind::MpegTsRtpSdpPublisher
                ? MediaRuntimeNodeFactory::createMpegTsRtpSdpPublisher(*node, authority.value())
                : MediaRuntimeNodeFactory::create(*node, nullptr, nullptr, authority.value(), serviceScopeArbiter);
            if (!created) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, created.error());
            if (auto* encoderResolver = dynamic_cast<CodecResolverNode*>(created.value().get())) {
                auto bound = encoderResolver->bindPreparedEncoder(prepared.encoder);
                if (!bound) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, bound.error());
            }
            if (auto* candidate = dynamic_cast<MediaScheduledDatagramSenderNode*>(created.value().get())) sender = candidate;
            nodes.push_back(std::move(created).value());
        }
        if (!sender) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation,
            ::media::ErrorInfo::notInitialized("prepared output has no datagram sender"));
        std::vector<MediaRuntimeSegmentOutputBinding> upstreamInputs;
        std::set<std::uint32_t> exportedPorts;
        const auto belongs = [&](MediaNodeId id) {
            return std::find(prepared.branch.nodeIds.begin(), prepared.branch.nodeIds.end(), id) != prepared.branch.nodeIds.end();
        };
        for (const auto& edge : publishedGraph->edges()) {
            if (!belongs(edge.to.nodeId) || belongs(edge.from.nodeId) ||
                !exportedPorts.insert(edge.from.portId.value).second) continue;
            auto binding = runtime.context().exportOutput(edge.from.portId);
            if (!binding) return reject(task.id, task.request.output.sdpPath,
                MediaRealtimeOutputFailureStage::Preparation, binding.error());
            upstreamInputs.push_back(std::move(binding).value());
        }
        auto branch = MediaRuntimeBranch::prepare({task.id, prepared.branch.threading, publishedGraph,
            prepared.branch.nodeIds, std::move(upstreamInputs), std::move(nodes), std::move(lease).value()}, runtime.context());
        if (!branch) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation, branch.error());
        MediaEdgeId frame;
        for (const auto id : branch.value()->inputEdges()) {
            const auto* edge = publishedGraph->findEdge(id);
            if (edge->from.nodeId == fanoutId) frame = id;
        }
        if (!frame.isValid()) return reject(task.id, task.request.output.sdpPath, MediaRealtimeOutputFailureStage::Preparation,
            ::media::ErrorInfo::notInitialized("prepared output lacks its fanout input edge"));
        auto inserted = outputs.emplace(task.id, Output{task.id, branch.value(), prepared.branch.nodeIds, frame,
            authority.value(),
            video.startup.maximumWait, task.generation, std::chrono::steady_clock::now(),
            {task.id, MediaRealtimeOutputState::WaitingForRandomAccess, MediaRealtimeOutputFailureStage::Publication,
                {}, task.request.output.sdpPath, std::nullopt}});
        auto& output = inserted.first->second;
        output.transactionDeadline = task.deadline;
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Preparation;
        auto started = output.branch->start();
        if (!started) { output.branch->fail(started.error()); return ::media::Status::success(); }
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Publication;
        for (const auto id : output.branch->inputEdges()) {
            if (id == frame) continue;
            const auto* edge = publishedGraph->findEdge(id);
            MediaBufferRef metadata;
            if (edge->from.nodeId == formatSource.node) metadata = task.source;
            else if (edge->from.nodeId == resolverId) metadata = task.timestamp;
            if (!metadata || output.branch->tryPublish(id, metadata).outcome != MediaQueuePushOutcome::Accepted) {
                output.branch->fail(::media::ErrorInfo::internalError("prepared output metadata replay was not accepted"));
                return ::media::Status::success();
            }
        }
        auto subscribed = fanout->subscribe(output.branch, frame);
        if (!subscribed) { output.branch->fail(subscribed.error()); return ::media::Status::success(); }
        graph = std::move(publishedGraph);
        ++graphVersion;
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Runtime;
        publish(output.snapshot);
        // Keep the change busy until the branch is ready or has physically retired.
        return ::media::Status::success();
    }

    ::media::Status drain(Output& output)
    {
        fanout->unsubscribe(output.id);
        auto generation = fanout->sourceGeneration();
        if (generation) output.generation = generation.value();
        output.snapshot.stage = MediaRealtimeOutputFailureStage::Drain;
        auto status = output.branch->beginDrain(output.generation, drainPlan);
        if (!status) output.branch->fail(status.error());
        else { output.snapshot.state = MediaRealtimeOutputState::Draining; output.snapshot.stage = MediaRealtimeOutputFailureStage::Drain; publish(output.snapshot); }
        return ::media::Status::success();
    }

    ::media::Status poll()
    {
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
        publishPendingTerminal();
        for (auto it = outputs.begin(); it != outputs.end();) {
            auto& output = it->second;
            if (output.snapshot.state == MediaRealtimeOutputState::WaitingForRandomAccess &&
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
            if (output.branch->failure()) fanout->unsubscribe(output.id);
            auto retired = output.branch->poll();
            if (!retired) {
                if (output.branch->state() == MediaRuntimeBranchState::Retiring ||
                    output.branch->state() == MediaRuntimeBranchState::Retired)
                    output.snapshot.stage = MediaRealtimeOutputFailureStage::Release;
                output.branch->fail(retired.error());
            }
            if (const auto failure = output.branch->failure(); failure && output.snapshot.state != MediaRealtimeOutputState::Failed) {
                fanout->unsubscribe(output.id);
                output.snapshot.state = MediaRealtimeOutputState::Failed;
                output.snapshot.detail = failure->error.message;
                output.snapshot.error = failure->error;
                if (finishing && !finishFailure) finishFailure = failure->error;
                publish(output.snapshot);
            }
            if (retired && retired.value()) {
                fanout->unsubscribe(output.id);
                addMetrics(retiredMetrics, output.branch->metrics());
                auto nextGraph = std::make_shared<MediaGraph>(*graph);
                nextGraph->removeNodes(output.nodes);
                graph = std::move(nextGraph);
                ++graphVersion;
                output.snapshot.state = MediaRealtimeOutputState::Retired;
                auto terminal = std::move(output.snapshot);
                it = outputs.erase(it);
                if (changingOutput == terminal.outputId) { control.completeOutputChange(); changingOutput.reset(); }
                publish(std::move(terminal));
            } else ++it;
        }
        if (!finishing && !preparation) {
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
        if (auto change = control.takeOutputChange()) reject(change->outputId,
            change->addition ? change->addition->output.sdpPath : std::string{}, MediaRealtimeOutputFailureStage::Publication,
            ::media::ErrorInfo::cancelled("session is finishing"));
        publishPendingTerminal();
        for (auto& [id, output] : outputs) {
            fanout->unsubscribe(id);
            if (!cause) output.branch->fail(cause.error());
            else if (output.branch->state() == MediaRuntimeBranchState::Running) drain(output);
        }
        while (preparation || !outputs.empty()) {
            poll();
            if (preparation || !outputs.empty()) std::this_thread::sleep_for(policy.pollInterval());
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
    // Retained through zero outputs; joining a branch never resets virtual time.
    std::shared_ptr<MediaDatagramServiceScopeArbiter> serviceScopeArbiter;
    VideoOutputFanoutNode* fanout = nullptr;
    CodecResolverNode* resolver = nullptr;
    MediaNodeId fanoutId;
    MediaNodeId resolverId;
    MediaEndpoint formatSource;
    std::map<std::uint64_t, Output> outputs;
    std::shared_ptr<Preparation> preparation;
    std::future<::media::Result<MediaPreparedRealtimeOutput>> preparationResult;
    std::optional<std::uint64_t> changingOutput;
    std::optional<MediaRealtimeOutputSnapshot> pendingTerminal;
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
        impl->publishPendingTerminal();
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
}

} // namespace media::ffmpeg::graph

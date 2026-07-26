#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"
#include "internal/graph/nodes/merge/PacketMergeNode.h"
#include "internal/graph/nodes/demux/DemuxNode.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/MediaNodeRuntime.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

template <typename Predicate>
bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

class PendingOutputTestNode final : public FFmpegNodeRuntime {
public:
    enum class Behavior {
        SingleFanout,
        DataThenEof,
        SingleThreadedOutput,
        CodecLikeBatch,
        SingleKeyOutput
    };

    PendingOutputTestNode(MediaNodeId nodeId, Behavior behavior)
        : FFmpegNodeRuntime(nodeId, MediaNodeKind::ControlSignal, "PendingOutputTestNode")
        , m_behavior(behavior)
    {
    }

    std::size_t processCalls() const noexcept { return m_processCalls.load(); }
    std::size_t pendingBuffers() const noexcept { return pendingOutputBufferCount(); }
    std::size_t emitAttempts() const noexcept { return m_emitAttempts.load(); }

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override
    {
        const std::size_t call = ++m_processCalls;
        if (m_behavior == Behavior::SingleKeyOutput) {
            if (call > 1) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waiting());
            }
            return emitPacket(context, 601, false, MediaNodeProcessResult::progress(), true);
        }
        if (m_behavior == Behavior::CodecLikeBatch) {
            for (std::int64_t pts = 400; pts < 410; ++pts) {
                ++m_emitAttempts;
                auto packet = makePacketBuffer(false, pts);
                if (!packet) return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
                auto status = pushToAllOutputs(context, packet.value());
                if (!status) return processProgress(std::move(status));
            }
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
        }
        if (m_behavior == Behavior::SingleFanout) {
            if (call > 1) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waiting());
            }
            return emitPacket(context, 101, false, MediaNodeProcessResult::progress());
        }
        if (m_behavior == Behavior::DataThenEof) {
            if (call == 1) {
                return emitPacket(context, 201, false, MediaNodeProcessResult::progress());
            }
            if (call == 2) {
                return emitPacket(context, 202, true, MediaNodeProcessResult::finished());
            }
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError("finished node was re-entered"));
        }
        if (call == 1) {
            return emitPacket(context, 301, false, MediaNodeProcessResult::progress());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

private:
    ::media::Result<MediaNodeProcessResult> emitPacket(MediaGraphExecutionContext& context,
                                                        std::int64_t pts,
                                                        bool eof,
                                                        MediaNodeProcessResult result,
                                                        bool keyFrame = false)
    {
        auto packet = makePacketBuffer(keyFrame, pts);
        if (!packet) {
            return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
        }
        if (eof) {
            packet.value()->addFlags(MediaBufferFlag::Eof);
        }
        auto status = pushToAllOutputs(context, packet.value());
        if (result.state == MediaNodeProcessState::Finished) {
            return processFinished(std::move(status));
        }
        return processProgress(std::move(status));
    }

    Behavior m_behavior;
    std::atomic_size_t m_processCalls{ 0 };
    std::atomic_size_t m_emitAttempts{ 0 };
};

MediaGraph makePendingOutputGraph(MediaNodeId& sourceId,
                                  MediaNodeId& firstSinkId,
                                  MediaNodeId* secondSinkId = nullptr,
                                  MediaQueueOverflowPolicy overflowPolicy = MediaQueueOverflowPolicy::BlockProducer)
{
    MediaGraph graph;
    sourceId = graph.addNode(MediaNodeKind::ControlSignal, "pending.source");
    firstSinkId = graph.addNode(MediaNodeKind::RtpOutput, "pending.sink.first");
    graph.addOutputPort(sourceId, "packet", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(firstSinkId, "packet", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    auto queuePolicy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    queuePolicy.queuePolicy.overflowPolicy = overflowPolicy;
    graph.connect(sourceId, "packet", firstSinkId, "packet", "pending first", queuePolicy);
    if (secondSinkId) {
        *secondSinkId = graph.addNode(MediaNodeKind::RtpOutput, "pending.sink.second");
        graph.addInputPort(*secondSinkId, "packet", MediaStreamKind::Video,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
        graph.connect(sourceId, "packet", *secondSinkId, "packet", "pending second",
                      MediaBlockingEdgePolicyPlanner::planQueue(1));
    }
    return graph;
}

void testPartialFanoutDoesNotDuplicateSuccessfulTarget(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId firstSinkId;
    MediaNodeId secondSinkId;
    MediaGraph graph = makePendingOutputGraph(sourceId, firstSinkId, &secondSinkId);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PendingOutputTestNode node(sourceId, PendingOutputTestNode::Behavior::SingleFanout);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* first = execution.findInputChannel(firstSinkId, "packet");
    MediaChannel* second = execution.findInputChannel(secondSinkId, "packet");
    auto filler = makePacketBuffer(false, 100);
    EXPECT_TRUE(ctx, first && second && filler &&
                         second->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);

    auto initial = node.process(execution);
    EXPECT_TRUE(ctx, initial && initial.value().state == MediaNodeProcessState::Waiting);
    MediaBufferRef firstValue;
    MediaBufferRef secondFiller;
    EXPECT_TRUE(ctx, first->tryPop(firstValue));
    EXPECT_TRUE(ctx, firstValue && firstValue->pts() == 101);
    EXPECT_TRUE(ctx, second->tryPop(secondFiller));
    EXPECT_TRUE(ctx, secondFiller && secondFiller->pts() == 100);

    auto drained = node.process(execution);
    EXPECT_TRUE(ctx, drained);
    MediaBufferRef secondValue;
    EXPECT_TRUE(ctx, second->tryPop(secondValue));
    EXPECT_TRUE(ctx, secondValue && secondValue->pts() == 101);
    MediaBufferRef duplicate;
    EXPECT_FALSE(ctx, first->tryPop(duplicate));
    EXPECT_FALSE(ctx, second->tryPop(duplicate));
    EXPECT_EQ(ctx, node.processCalls(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, node.stop(execution));
}

void testFinishedOutputDrainsInOrderBeforeCloseWithoutReentry(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraph graph = makePendingOutputGraph(sourceId, sinkId);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PendingOutputTestNode node(sourceId, PendingOutputTestNode::Behavior::DataThenEof);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* output = execution.findInputChannel(sinkId, "packet");
    auto filler = makePacketBuffer(false, 200);
    EXPECT_TRUE(ctx, output && filler &&
                         output->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);

    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, node.processCalls(), static_cast<std::size_t>(1));
    EXPECT_FALSE(ctx, output->closed());
    MediaBufferRef value;
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 200);

    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, node.processCalls(), static_cast<std::size_t>(1));
    EXPECT_FALSE(ctx, output->closed());
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 201 && !value->isEof());

    auto finished = node.process(execution);
    EXPECT_TRUE(ctx, finished && finished.value().state == MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, node.processCalls(), static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, output->closed());
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 202 && value->isEof());
    EXPECT_TRUE(ctx, node.stop(execution));
}

void testProducerPopWakesPendingThreadedOutputWithoutBusySpin(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraph graph = makePendingOutputGraph(sourceId, sinkId);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PendingOutputTestNode node(sourceId, PendingOutputTestNode::Behavior::SingleThreadedOutput);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* output = execution.findInputChannel(sinkId, "packet");
    auto filler = makePacketBuffer(false, 300);
    EXPECT_TRUE(ctx, output && filler &&
                         output->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);
    MediaGraphWorker worker(node, execution);
    EXPECT_TRUE(ctx, worker.start());
    EXPECT_TRUE(ctx, waitUntil([&] { return worker.metrics().waits.load() >= 1; }));
    const auto callsWhileBlocked = worker.metrics().processCalls.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(ctx, worker.metrics().processCalls.load(), callsWhileBlocked);

    MediaBufferRef value;
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 300);
    const bool workerFinished = waitUntil([&] { return !worker.running(); });
    EXPECT_TRUE(ctx, workerFinished);
    if (!workerFinished) {
        worker.requestStop();
    }
    worker.join();
    EXPECT_TRUE(ctx, worker.metrics().wakeups.load() >= 1);
    EXPECT_EQ(ctx, node.processCalls(), static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 301);
    EXPECT_TRUE(ctx, output->closed());
    EXPECT_TRUE(ctx, node.stop(execution));
}

void testCodecLikeBatchStopsAtOnePendingBuffer(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraph graph = makePendingOutputGraph(sourceId, sinkId);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PendingOutputTestNode node(sourceId, PendingOutputTestNode::Behavior::CodecLikeBatch);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* output = execution.findInputChannel(sinkId, "packet");
    auto filler = makePacketBuffer(false, 399);
    EXPECT_TRUE(ctx, output && filler &&
                         output->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);
    auto result = node.process(execution);
    EXPECT_TRUE(ctx, result && result.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, node.pendingBuffers(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, node.emitAttempts(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, node.stop(execution));
}

void testDroppedOutputIsNotRetried(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraph graph = makePendingOutputGraph(
        sourceId, sinkId, nullptr, MediaQueueOverflowPolicy::DropNewest);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PendingOutputTestNode node(sourceId, PendingOutputTestNode::Behavior::SingleFanout);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* output = execution.findInputChannel(sinkId, "packet");
    auto filler = makePacketBuffer(false, 500);
    EXPECT_TRUE(ctx, output && filler &&
                         output->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);
    auto result = node.process(execution);
    EXPECT_TRUE(ctx, result && result.value().state == MediaNodeProcessState::Progress);
    EXPECT_EQ(ctx, node.pendingBuffers(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, output->metrics().queue.dropped.load(), static_cast<std::uint64_t>(1));
    EXPECT_TRUE(ctx, node.stop(execution));

    MediaGraph dropNonKeyGraph = makePendingOutputGraph(
        sourceId, sinkId, nullptr, MediaQueueOverflowPolicy::DropNonKeyFrame);
    MediaGraphExecutionContext dropNonKeyExecution;
    EXPECT_TRUE(ctx, dropNonKeyExecution.compile(dropNonKeyGraph));
    PendingOutputTestNode dropNonKeyNode(sourceId, PendingOutputTestNode::Behavior::SingleFanout);
    EXPECT_TRUE(ctx, dropNonKeyNode.start(dropNonKeyExecution));
    output = dropNonKeyExecution.findInputChannel(sinkId, "packet");
    filler = makePacketBuffer(true, 510);
    EXPECT_TRUE(ctx, output && filler &&
                         output->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);
    result = dropNonKeyNode.process(dropNonKeyExecution);
    EXPECT_TRUE(ctx, result && result.value().state == MediaNodeProcessState::Progress);
    EXPECT_EQ(ctx, dropNonKeyNode.pendingBuffers(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, output->metrics().queue.dropped.load(), static_cast<std::uint64_t>(1));
    EXPECT_TRUE(ctx, dropNonKeyNode.stop(dropNonKeyExecution));
}

void testDropNonKeyFramePreservesBlockedKeyFrameInSinglePending(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraph graph = makePendingOutputGraph(
        sourceId, sinkId, nullptr, MediaQueueOverflowPolicy::DropNonKeyFrame);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PendingOutputTestNode node(sourceId, PendingOutputTestNode::Behavior::SingleKeyOutput);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* output = execution.findInputChannel(sinkId, "packet");
    auto filler = makePacketBuffer(true, 600);
    EXPECT_TRUE(ctx, output && filler &&
                         output->pushOutcome(filler.value()) == MediaQueuePushOutcome::Accepted);
    auto result = node.process(execution);
    EXPECT_TRUE(ctx, result && result.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, node.pendingBuffers(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, output->metrics().queue.dropped.load(), static_cast<std::uint64_t>(0));
    MediaBufferRef value;
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, node.pendingBuffers(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, output->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 601 && value->isKeyFrame());
    EXPECT_TRUE(ctx, node.stop(execution));
}

class FinishCountingNode final : public MediaNodeRuntime {
public:
    FinishCountingNode(MediaNodeId nodeId, std::size_t& processCalls)
        : MediaNodeRuntime(nodeId, MediaNodeKind::ControlSignal, "FinishCountingNode")
        , m_processCalls(processCalls)
    {
    }
protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext&) override
    {
        ++m_processCalls;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

private:
    std::size_t& m_processCalls;
};

MediaGraph makeFinishedNodeGraph(MediaNodeId& sourceId, MediaNodeId& sinkId)
{
    MediaGraph graph;
    sourceId = graph.addNode(MediaNodeKind::ControlSignal, "finished.source");
    sinkId = graph.addNode(MediaNodeKind::ControlSignal, "finished.sink");
    graph.addOutputPort(sourceId, "event", MediaStreamKind::Control,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    graph.addInputPort(sinkId, "event", MediaStreamKind::Control,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    graph.connect(sourceId, "event", sinkId, "event", "finished event",
                  MediaBlockingEdgePolicyPlanner::planQueue(1));
    return graph;
}

void testSchedulerRetiresFinishedNodes(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraph graph = makeFinishedNodeGraph(sourceId, sinkId);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));

    std::size_t sourceCalls = 0;
    std::size_t sinkCalls = 0;
    MediaGraphScheduler scheduler;
    EXPECT_TRUE(ctx, scheduler.registerNode(std::make_unique<FinishCountingNode>(sourceId, sourceCalls)));
    EXPECT_TRUE(ctx, scheduler.registerNode(std::make_unique<FinishCountingNode>(sinkId, sinkCalls)));
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, scheduler.processSchedulingStep(execution));
    EXPECT_TRUE(ctx, scheduler.processSchedulingStep(execution));

    MediaChannel* channel = execution.findInputChannel(sinkId, "event");
    EXPECT_EQ(ctx, sourceCalls, static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, sinkCalls, static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, channel);
    if (channel) {
        EXPECT_EQ(ctx, channel->metrics().closed.load(), static_cast<std::uint64_t>(1));
    }

    EXPECT_TRUE(ctx, scheduler.stop(execution));
    EXPECT_TRUE(ctx, scheduler.start(execution));
    EXPECT_TRUE(ctx, scheduler.processSchedulingStep(execution));
    EXPECT_EQ(ctx, sourceCalls, static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, sinkCalls, static_cast<std::size_t>(2));
}

void testSingleThreadRuntimeCompletesAfterNodesFinish(TestContext& ctx)
{
    MediaNodeId sourceId;
    MediaNodeId sinkId;
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(makeFinishedNodeGraph(sourceId, sinkId)));

    std::size_t sourceCalls = 0;
    std::size_t sinkCalls = 0;
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(std::make_unique<FinishCountingNode>(sourceId, sourceCalls)));
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(std::make_unique<FinishCountingNode>(sinkId, sinkCalls)));
    auto result = runtime.run();

    EXPECT_TRUE(ctx, result);
    if (result) {
        EXPECT_TRUE(ctx, result.value().completed);
        EXPECT_EQ(ctx, result.value().totalClosed, static_cast<std::uint64_t>(1));
    }
    EXPECT_EQ(ctx, sourceCalls, static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, sinkCalls, static_cast<std::size_t>(1));

    MediaNodeId reusedSourceId;
    MediaNodeId reusedSinkId;
    EXPECT_TRUE(ctx, runtime.compile(makeFinishedNodeGraph(reusedSourceId, reusedSinkId)));
    EXPECT_EQ(ctx, reusedSourceId.value, sourceId.value);
    EXPECT_EQ(ctx, reusedSinkId.value, sinkId.value);
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
                         std::make_unique<FinishCountingNode>(reusedSourceId, sourceCalls)));
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(
                         std::make_unique<FinishCountingNode>(reusedSinkId, sinkCalls)));
    auto reusedResult = runtime.run();
    EXPECT_TRUE(ctx, reusedResult && reusedResult.value().completed);
    EXPECT_EQ(ctx, sourceCalls, static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, sinkCalls, static_cast<std::size_t>(2));
}

void testTerminalAndMuxCompletionState(TestContext& ctx)
{
    MediaInputTerminalTracker tracker({ "video", "audio" });
    EXPECT_TRUE(ctx, tracker.markEof("video"));
    EXPECT_FALSE(ctx, tracker.markEof("video"));
    EXPECT_FALSE(ctx, tracker.finished());
    EXPECT_TRUE(ctx, tracker.markEof("audio"));
    EXPECT_TRUE(ctx, tracker.finished());

    MediaMuxCompletionState completion;
    completion.setExpectedConfigKeys({ "video", "audio", "audio" });
    completion.setExpectedTerminalChannels({ "video", "audio", "audio" });
    EXPECT_FALSE(ctx, completion.markConfigReady("unknown-video"));
    EXPECT_FALSE(ctx, completion.markConfigReady("unknown-audio"));
    completion.markHeaderWritten();
    completion.markInputEof("video");
    completion.markInputEof("audio");
    EXPECT_FALSE(ctx, completion.readyForTrailer());
    completion.markConfigReady("video");
    completion.markConfigReady("audio");
    completion.setPendingPackets(1);
    EXPECT_FALSE(ctx, completion.readyForTrailer());
    completion.setPendingPackets(0);
    EXPECT_TRUE(ctx, completion.readyForTrailer());
    EXPECT_FALSE(ctx, completion.finished());
    completion.markTrailerWritten();
    EXPECT_TRUE(ctx, completion.finished());
}


void testPacketMergeWaitsForEveryInputTerminal(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId sourceA = graph.addNode(MediaNodeKind::Demux, "merge.source.a");
    const MediaNodeId sourceB = graph.addNode(MediaNodeKind::Demux, "merge.source.b");
    const MediaNodeId mergeId = graph.addNode(MediaNodeKind::PacketMerge, "merge");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::RtpOutput, "merge.sink");
    for (MediaNodeId source : {sourceA, sourceB})
        graph.addOutputPort(source, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(mergeId, "a", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(mergeId, "b", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(mergeId, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(sink, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(8);
    graph.connect(sourceA, "packet", mergeId, "a", "merge a", policy);
    graph.connect(sourceB, "packet", mergeId, "b", "merge b", policy);
    graph.connect(mergeId, "packet", sink, "packet", "merged", policy);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    PacketMergeNode merge(mergeId);
    auto eof = makePacketBuffer(false, 1);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    eof.value()->addFlags(MediaBufferFlag::Eof);
    MediaChannel* inputA = execution.findInputChannel(mergeId, "a");
    MediaChannel* inputB = execution.findInputChannel(mergeId, "b");
    MediaChannel* output = execution.findInputChannel(sink, "packet");
    for (std::int64_t pts = 0; pts < 8; ++pts) {
        auto packet = makePacketBuffer(false, pts);
        EXPECT_TRUE(ctx, packet && inputA->push(packet.value()));
    }

    std::atomic_bool producersOk{ true };
    std::thread busyProducer([&] {
        for (std::int64_t pts = 8; pts < 256; ++pts) {
            auto packet = makePacketBuffer(false, pts);
            if (!packet || !inputA->push(packet.value())) producersOk = false;
        }
        if (!inputA->push(eof.value())) producersOk = false;
    });
    std::thread sparseProducer([&] {
        for (std::int64_t pts : { -3, -2, -1 }) {
            auto packet = makePacketBuffer(false, pts);
            if (!packet || !inputB->push(packet.value())) producersOk = false;
        }
        if (!inputB->push(eof.value())) producersOk = false;
    });
    MediaGraphWorker worker(merge, execution);
    EXPECT_TRUE(ctx, worker.start());

    std::size_t outputIndex = 0;
    std::size_t busyCount = 0;
    std::size_t sparseCount = 0;
    std::size_t firstSparseIndex = 999;
    for (;;) {
        MediaBufferRef packet;
        EXPECT_TRUE(ctx, output->pop(packet));
        if (!packet || packet->isEof()) break;
        if (packet->pts() < 0) {
            if (firstSparseIndex == 999) firstSparseIndex = outputIndex;
            ++sparseCount;
        } else {
            ++busyCount;
        }
        ++outputIndex;
    }
    busyProducer.join();
    sparseProducer.join();
    worker.join();
    EXPECT_TRUE(ctx, producersOk.load());
    EXPECT_EQ(ctx, busyCount, static_cast<std::size_t>(256));
    EXPECT_EQ(ctx, sparseCount, static_cast<std::size_t>(3));
    EXPECT_TRUE(ctx, firstSparseIndex < static_cast<std::size_t>(16));
    MediaBufferRef extra;
    EXPECT_FALSE(ctx, output && output->tryPop(extra));
}

void testPacketMergeSameInstanceRestartsWithoutTerminalState(TestContext& ctx)
{
    MediaNodeId mergeId;
    MediaNodeId sinkId;
    auto makeExecution = [&](MediaGraph& graph, MediaGraphExecutionContext& execution) {
        const MediaNodeId sourceA = graph.addNode(MediaNodeKind::Demux, "restart.source.a");
        const MediaNodeId sourceB = graph.addNode(MediaNodeKind::Demux, "restart.source.b");
        mergeId = graph.addNode(MediaNodeKind::PacketMerge, "restart.merge");
        sinkId = graph.addNode(MediaNodeKind::RtpOutput, "restart.sink");
        for (MediaNodeId source : {sourceA, sourceB})
            graph.addOutputPort(source, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
        graph.addInputPort(mergeId, "a", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
        graph.addInputPort(mergeId, "b", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
        graph.addOutputPort(mergeId, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
        graph.addInputPort(sinkId, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
        const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(8);
        graph.connect(sourceA, "packet", mergeId, "a", "restart a", policy);
        graph.connect(sourceB, "packet", mergeId, "b", "restart b", policy);
        graph.connect(mergeId, "packet", sinkId, "packet", "restart merged", policy);
        return execution.compile(graph);
    };

    MediaGraph firstGraph;
    MediaGraphExecutionContext first;
    EXPECT_TRUE(ctx, makeExecution(firstGraph, first));
    PacketMergeNode merge(mergeId);
    EXPECT_TRUE(ctx, merge.start(first));
    auto eof = makePacketBuffer(false, 10);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    eof.value()->addFlags(MediaBufferFlag::Eof);
    MediaGraphWorker firstWorker(merge, first);
    EXPECT_TRUE(ctx, firstWorker.start());
    EXPECT_TRUE(ctx, first.findInputChannel(mergeId, "a")->push(eof.value()));
    EXPECT_TRUE(ctx, first.findInputChannel(mergeId, "b")->push(eof.value()));
    firstWorker.join();
    EXPECT_TRUE(ctx, merge.stop(first));

    MediaGraph secondGraph;
    MediaGraphExecutionContext second;
    EXPECT_TRUE(ctx, makeExecution(secondGraph, second));
    EXPECT_TRUE(ctx, merge.start(second));
    auto tail = makePacketBuffer(false, 20);
    EXPECT_TRUE(ctx, tail);
    if (!tail) return;
    MediaGraphWorker secondWorker(merge, second);
    EXPECT_TRUE(ctx, secondWorker.start());
    EXPECT_TRUE(ctx, second.findInputChannel(mergeId, "a")->push(tail.value()));
    EXPECT_TRUE(ctx, waitUntil([&] { return secondWorker.metrics().progress.load() >= 1; }));
    EXPECT_TRUE(ctx, secondWorker.running());
    EXPECT_TRUE(ctx, second.findInputChannel(mergeId, "a")->push(eof.value()));
    EXPECT_TRUE(ctx, second.findInputChannel(mergeId, "b")->push(eof.value()));
    secondWorker.join();
    MediaBufferRef firstOutput;
    EXPECT_TRUE(ctx, second.findInputChannel(sinkId, "packet")->tryPop(firstOutput));
    EXPECT_TRUE(ctx, firstOutput && !firstOutput->isEof() && firstOutput->pts() == 20);
    EXPECT_TRUE(ctx, merge.stop(second));
}

} // namespace

void runEventRuntimeMultiInputTests(media_transcode::test::TestContext& ctx)
{
    testPartialFanoutDoesNotDuplicateSuccessfulTarget(ctx);
    testFinishedOutputDrainsInOrderBeforeCloseWithoutReentry(ctx);
    testProducerPopWakesPendingThreadedOutputWithoutBusySpin(ctx);
    testCodecLikeBatchStopsAtOnePendingBuffer(ctx);
    testDroppedOutputIsNotRetried(ctx);
    testDropNonKeyFramePreservesBlockedKeyFrameInSinglePending(ctx);
    testSchedulerRetiresFinishedNodes(ctx);
    testSingleThreadRuntimeCompletesAfterNodesFinish(ctx);
    testTerminalAndMuxCompletionState(ctx);
    testPacketMergeWaitsForEveryInputTerminal(ctx);
    testPacketMergeSameInstanceRestartsWithoutTerminalState(ctx);
}

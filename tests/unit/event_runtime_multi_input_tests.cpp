#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"
#include "internal/graph/nodes/merge/PacketMergeNode.h"
#include "internal/graph/nodes/demux/DemuxNode.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
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
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
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
        const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
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
    testTerminalAndMuxCompletionState(ctx);
    testPacketMergeWaitsForEveryInputTerminal(ctx);
    testPacketMergeSameInstanceRestartsWithoutTerminalState(ctx);
}

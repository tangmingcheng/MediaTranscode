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

std::string readSource(const char* path)
{
    const std::filesystem::path repository = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    std::ifstream input(repository / path, std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

class WaitingTestNode final : public MediaRuntimeNode {
public:
    explicit WaitingTestNode(MediaNodeId id) : m_id(id) {}
    MediaNodeId nodeId() const noexcept override { return m_id; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext&) override
    {
        ++calls;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }
    std::atomic_uint64_t calls{ 0 };
private:
    MediaNodeId m_id;
};

class TerminalTestNode final : public MediaRuntimeNode {
public:
    TerminalTestNode(MediaNodeId id, bool fail) : m_id(id), m_fail(fail) {}
    MediaNodeId nodeId() const noexcept override { return m_id; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext&) override
    {
        if (m_fail) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError("terminal test failure"));
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }
private:
    MediaNodeId m_id;
    bool m_fail = false;
};

class LostWakeRaceNode final : public MediaRuntimeNode {
public:
    explicit LostWakeRaceNode(MediaNodeId id) : m_id(id) {}
    MediaNodeId nodeId() const noexcept override { return m_id; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override
    {
        if (++calls == 1) {
            context.nodeWakeup(m_id).notify();
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }
    std::atomic_uint64_t calls{ 0 };
private:
    MediaNodeId m_id;
};

void testWaitingWorkerBlocksUntilDirectedWakeup(TestContext& ctx)
{
    MediaGraphExecutionContext execution;
    WaitingTestNode node(MediaNodeId::fromValue(77));
    MediaGraphWorker worker(node, execution);
    EXPECT_TRUE(ctx, worker.start());
    EXPECT_TRUE(ctx, waitUntil([&] {
        return node.calls.load() == 1 && worker.metrics().waits.load() == 1;
    }));
    EXPECT_EQ(ctx, node.calls.load(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, worker.metrics().waits.load(), static_cast<std::uint64_t>(1));
    execution.nodeWakeup(node.nodeId()).notify();
    EXPECT_TRUE(ctx, waitUntil([&] {
        return node.calls.load() == 2 && worker.metrics().wakeups.load() == 1;
    }));
    EXPECT_EQ(ctx, node.calls.load(), static_cast<std::uint64_t>(2));
    EXPECT_EQ(ctx, worker.metrics().wakeups.load(), static_cast<std::uint64_t>(1));
    worker.requestStop();
    worker.join();
    EXPECT_FALSE(ctx, worker.running());
}

void testFinishedAndErrorWorkerMetrics(TestContext& ctx)
{
    MediaGraphExecutionContext execution;
    TerminalTestNode finished(MediaNodeId::fromValue(78), false);
    MediaGraphWorker finishedWorker(finished, execution);
    EXPECT_TRUE(ctx, finishedWorker.start());
    finishedWorker.join();
    EXPECT_EQ(ctx, finishedWorker.metrics().processCalls.load(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, finishedWorker.metrics().errors.load(), static_cast<std::uint64_t>(0));

    TerminalTestNode failed(MediaNodeId::fromValue(79), true);
    MediaGraphWorker failedWorker(failed, execution);
    EXPECT_TRUE(ctx, failedWorker.start());
    failedWorker.join();
    EXPECT_EQ(ctx, failedWorker.metrics().processCalls.load(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, failedWorker.metrics().errors.load(), static_cast<std::uint64_t>(1));
    EXPECT_TRUE(ctx, failedWorker.aborted());
}

void testWakeupBetweenSnapshotAndWaitingIsNotLost(TestContext& ctx)
{
    MediaGraphExecutionContext execution;
    LostWakeRaceNode node(MediaNodeId::fromValue(80));
    MediaGraphWorker worker(node, execution);
    EXPECT_TRUE(ctx, worker.start());
    worker.join();
    EXPECT_EQ(ctx, node.calls.load(), static_cast<std::uint64_t>(2));
    EXPECT_EQ(ctx, worker.metrics().waits.load(), static_cast<std::uint64_t>(1));
}

void testExecutorConstructsAllWorkersBeforeConcurrentStart(TestContext& ctx)
{
    constexpr std::uint32_t nodeCount = 64;
    constexpr std::uint32_t rounds = 20;
    for (std::uint32_t round = 0; round < rounds; ++round) {
        MediaGraph graph;
        MediaGraphScheduler scheduler;
        for (std::uint32_t index = 0; index < nodeCount; ++index) {
            const MediaNodeId id = graph.addNode(MediaNodeKind::PacketMerge, "start.stress");
            EXPECT_TRUE(ctx, scheduler.registerNode(std::make_unique<WaitingTestNode>(id)));
        }
        MediaGraphExecutionContext execution;
        EXPECT_TRUE(ctx, execution.compile(graph));
        MediaGraphThreadedExecutor executor;
        EXPECT_TRUE(ctx, executor.start(execution, scheduler));
        EXPECT_TRUE(ctx, waitUntil([&] {
            return executor.metrics().workerWaits == nodeCount;
        }));
        EXPECT_TRUE(ctx, executor.stop(execution, scheduler));
    }
}

void testChannelsWakeOnlyTheirConsumersAndAnyInputWakes(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId sourceA = graph.addNode(MediaNodeKind::Demux, "wake.source.a");
    const MediaNodeId sourceB = graph.addNode(MediaNodeKind::Demux, "wake.source.b");
    const MediaNodeId consumerA = graph.addNode(MediaNodeKind::PacketMerge, "wake.consumer.a");
    const MediaNodeId consumerB = graph.addNode(MediaNodeKind::PacketMerge, "wake.consumer.b");
    graph.addOutputPort(sourceA, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(sourceB, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(consumerA, "a", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(consumerA, "b", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(consumerB, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(4);
    graph.connect(sourceA, "packet", consumerA, "a", "wake a", policy);
    graph.connect(sourceB, "packet", consumerA, "b", "wake b", policy);
    graph.connect(sourceB, "packet", consumerB, "packet", "wake other", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    auto packet = makePacketBuffer(true);
    EXPECT_TRUE(ctx, packet);
    if (!packet) return;
    auto& wakeA = execution.nodeWakeup(consumerA);
    auto& wakeB = execution.nodeWakeup(consumerB);
    const auto a0 = wakeA.sequence();
    const auto b0 = wakeB.sequence();
    MediaChannel* first = execution.findInputChannel(consumerA, "a");
    EXPECT_TRUE(ctx, first != nullptr && first->push(packet.value()));
    EXPECT_TRUE(ctx, wakeA.sequence() != a0);
    EXPECT_EQ(ctx, wakeB.sequence(), b0);
    const auto a1 = wakeA.sequence();
    MediaChannel* second = execution.findInputChannel(consumerA, "b");
    EXPECT_TRUE(ctx, second != nullptr && second->push(packet.value()));
    EXPECT_TRUE(ctx, wakeA.sequence() != a1);
}

void testSpscAbortReleasesBlockedProducerAndConsumer(TestContext& ctx)
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::SpscRing;
    policy.capacity = 1;
    policy.bounded = true;
    policy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    MediaSpscRingQueue empty(policy);
    bool popOk = true;
    std::atomic_bool consumerStarted = false;
    std::thread consumer([&] { consumerStarted = true; MediaBufferRef out; popOk = static_cast<bool>(empty.pop(out)); });
    EXPECT_TRUE(ctx, waitUntil([&] { return consumerStarted.load(); }));
    empty.abort();
    consumer.join();
    EXPECT_FALSE(ctx, popOk);

    MediaSpscRingQueue full(policy);
    auto packet = makePacketBuffer(true);
    EXPECT_TRUE(ctx, packet && full.push(packet.value()));
    bool pushOk = true;
    std::atomic_bool producerStarted = false;
    std::thread producer([&] { producerStarted = true; pushOk = static_cast<bool>(full.push(packet.value())); });
    EXPECT_TRUE(ctx, waitUntil([&] { return producerStarted.load(); }));
    full.abort();
    producer.join();
    EXPECT_FALSE(ctx, pushOk);
}

} // namespace

void runEventRuntimeThreadingQueueTests(media_transcode::test::TestContext& ctx)
{
    testWaitingWorkerBlocksUntilDirectedWakeup(ctx);
    testFinishedAndErrorWorkerMetrics(ctx);
    testWakeupBetweenSnapshotAndWaitingIsNotLost(ctx);
    testExecutorConstructsAllWorkersBeforeConcurrentStart(ctx);
    testChannelsWakeOnlyTheirConsumersAndAnyInputWakes(ctx);
    testSpscAbortReleasesBlockedProducerAndConsumer(ctx);
}

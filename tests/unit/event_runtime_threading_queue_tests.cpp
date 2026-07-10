#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"
#include "common/DeterministicConcurrency.h"
#include "common/DeterministicRuntimeFault.h"

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
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include <sstream>
#include <thread>

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::DeterministicGate;
using media_transcode::test::DeterministicRuntimeFaultNode;
using media_transcode::test::RuntimeFaultStep;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

class ManualAcceptanceClock final : public MediaRuntimeAcceptanceClock {
public:
    time_point now() const noexcept override { return current; }
    void advance(std::chrono::milliseconds delta) noexcept { current += delta; }
private:
    time_point current{};
};

class FixedPlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        return ::media::Result<MediaRuntimePlatformSample>::success({ 6.0, 2.0, 9, 4096, true });
    }
};

class BaselineThenCpuPlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        const bool valid = calls++ != 0;
        return ::media::Result<MediaRuntimePlatformSample>::success({ 10.0, 4.0, 7, 2048, valid });
    }
private:
    std::size_t calls = 0;
};

class FailingPlatformSampler final : public MediaRuntimePlatformSampler {
public:
    ::media::Result<MediaRuntimePlatformSample> sample() noexcept override
    {
        return ::media::Result<MediaRuntimePlatformSample>::failure(
            ::media::ErrorInfo::ioFailure("deterministic platform sample failure"));
    }
};

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

class RunningStateOperationNode final : public MediaRuntimeNode {
public:
    RunningStateOperationNode(MediaNodeId id, std::function<bool()> operation)
        : m_id(id), m_operation(std::move(operation)) {}
    MediaNodeId nodeId() const noexcept override { return m_id; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext&) override
    {
        result = m_operation();
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }
    bool result = false;
private:
    MediaNodeId m_id;
    std::function<bool()> m_operation;
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
    DeterministicRuntimeFaultNode finished(MediaNodeId::fromValue(78), { RuntimeFaultStep::Finished });
    MediaGraphWorker finishedWorker(finished, execution);
    EXPECT_TRUE(ctx, finishedWorker.start());
    finishedWorker.join();
    EXPECT_EQ(ctx, finishedWorker.metrics().processCalls.load(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, finishedWorker.metrics().errors.load(), static_cast<std::uint64_t>(0));

    DeterministicRuntimeFaultNode failed(MediaNodeId::fromValue(79), { RuntimeFaultStep::Failure });
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

void testSpscCloseDeterministicallyReleasesBlockedEndpoints(TestContext& ctx)
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::SpscRing;
    policy.capacity = 1;
    policy.bounded = true;
    policy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;

    MediaSpscRingQueue empty(policy);
    DeterministicGate consumerEntered;
    bool popOk = true;
    std::thread consumer([&] {
        consumerEntered.arrive();
        MediaBufferRef out;
        popOk = static_cast<bool>(empty.pop(out));
    });
    consumerEntered.waitForArrivals(1);
    EXPECT_TRUE(ctx, waitUntil([&] { return empty.metrics().blockedConsumers.load() == 1; }));
    empty.close();
    consumer.join();
    EXPECT_FALSE(ctx, popOk);
    EXPECT_TRUE(ctx, empty.closed());
    EXPECT_FALSE(ctx, empty.aborted());

    MediaSpscRingQueue full(policy);
    auto packet = makePacketBuffer(true);
    EXPECT_TRUE(ctx, packet && full.push(packet.value()));
    if (!packet) return;
    DeterministicGate producerEntered;
    bool pushOk = true;
    std::thread producer([&] {
        producerEntered.arrive();
        pushOk = static_cast<bool>(full.push(packet.value()));
    });
    producerEntered.waitForArrivals(1);
    EXPECT_TRUE(ctx, waitUntil([&] { return full.metrics().blockedProducers.load() == 1; }));
    full.close();
    producer.join();
    EXPECT_FALSE(ctx, pushOk);
    EXPECT_TRUE(ctx, full.closed());
    EXPECT_FALSE(ctx, full.aborted());
}

void testRuntimeMetricsCarriesAcceptanceSamples(TestContext& ctx)
{
    MediaGraphRuntimeMetrics metrics;
    metrics.updateThreadCount(4, 3);
    metrics.updateQueuedBuffers(2);
    metrics.updateQueuedBuffers(7);
    metrics.updateQueuedBuffers(2);

    EXPECT_EQ(ctx, metrics.threadCount, static_cast<std::size_t>(4));
    EXPECT_EQ(ctx, metrics.activeWorkers, static_cast<std::size_t>(3));
    EXPECT_EQ(ctx, metrics.queuedBuffers, static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, metrics.peakQueuedBuffers, static_cast<std::size_t>(7));
}

void testAcceptanceCollectorUsesInjectedClockForFiveSecondStall(TestContext& ctx)
{
    auto clock = std::make_unique<ManualAcceptanceClock>();
    ManualAcceptanceClock* clockView = clock.get();
    MediaRuntimeAcceptanceCollector collector(
        std::move(clock), std::make_unique<FixedPlatformSampler>(), std::chrono::seconds(5));

    collector.sample(42);
    clockView->advance(std::chrono::milliseconds(4999));
    collector.sample(42);
    EXPECT_EQ(ctx, collector.snapshot().stalledIntervals, static_cast<std::uint64_t>(0));
    clockView->advance(std::chrono::milliseconds(2));
    collector.sample(42);
    const MediaGraphRuntimeMetrics metrics = collector.snapshot();
    EXPECT_EQ(ctx, metrics.stalledIntervals, static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, metrics.cpuSampleCount, static_cast<std::uint64_t>(3));
    EXPECT_NEAR(ctx, metrics.averageCpuPercent, 6.0, 0.001);
    EXPECT_NEAR(ctx, metrics.averageProcessCpuPercent, 2.0, 0.001);
    EXPECT_EQ(ctx, metrics.processThreadCount, static_cast<std::size_t>(9));
    EXPECT_EQ(ctx, metrics.workingSetBytes, static_cast<std::uint64_t>(4096));
}

void testAcceptanceCollectorExcludesBaselineAndPropagatesPlatformFailure(TestContext& ctx)
{
    MediaRuntimeAcceptanceCollector baselineCollector(
        std::make_unique<ManualAcceptanceClock>(),
        std::make_unique<BaselineThenCpuPlatformSampler>());
    EXPECT_TRUE(ctx, baselineCollector.sample(1));
    EXPECT_EQ(ctx, baselineCollector.snapshot().cpuSampleCount, static_cast<std::uint64_t>(0));
    EXPECT_TRUE(ctx, baselineCollector.sample(2));
    EXPECT_EQ(ctx, baselineCollector.snapshot().cpuSampleCount, static_cast<std::uint64_t>(1));
    EXPECT_NEAR(ctx, baselineCollector.snapshot().averageCpuPercent, 10.0, 0.001);

    MediaRuntimeAcceptanceCollector failingCollector(
        std::make_unique<ManualAcceptanceClock>(),
        std::make_unique<FailingPlatformSampler>());
    auto status = failingCollector.sample(1);
    EXPECT_FALSE(ctx, status);
    EXPECT_EQ(ctx, failingCollector.snapshot().errorCount, static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, failingCollector.snapshot().cpuSampleCount, static_cast<std::uint64_t>(0));
}

void testScriptedWaitingWakeProgressTerminalSequences(TestContext& ctx)
{
    MediaGraphExecutionContext execution;
    DeterministicRuntimeFaultNode completed(MediaNodeId::fromValue(81), {
        RuntimeFaultStep::Waiting, RuntimeFaultStep::Progress, RuntimeFaultStep::Finished
    });
    MediaGraphWorker completedWorker(completed, execution);
    EXPECT_TRUE(ctx, completedWorker.start());
    EXPECT_TRUE(ctx, waitUntil([&] { return completedWorker.metrics().waits.load() == 1; }));
    execution.nodeWakeup(completed.nodeId()).notify();
    completedWorker.join();
    EXPECT_EQ(ctx, completedWorker.metrics().progress.load(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, completedWorker.metrics().errors.load(), static_cast<std::uint64_t>(0));

    DeterministicRuntimeFaultNode failed(MediaNodeId::fromValue(82), {
        RuntimeFaultStep::Waiting, RuntimeFaultStep::Failure
    });
    MediaGraphWorker failedWorker(failed, execution);
    EXPECT_TRUE(ctx, failedWorker.start());
    EXPECT_TRUE(ctx, waitUntil([&] { return failedWorker.metrics().waits.load() == 1; }));
    execution.nodeWakeup(failed.nodeId()).notify();
    failedWorker.join();
    EXPECT_TRUE(ctx, failedWorker.aborted());
    EXPECT_EQ(ctx, failedWorker.metrics().errors.load(), static_cast<std::uint64_t>(1));
}

void testRuntimeLifecycleTransitionMatrix(TestContext& ctx)
{
    enum class Operation { Compile, Run, StartThreaded, Stop };
    struct Case { MediaGraphRuntimeState state; Operation operation; bool allowed; };
    const Case cases[] = {
        { MediaGraphRuntimeState::Empty, Operation::Compile, true },
        { MediaGraphRuntimeState::Empty, Operation::Run, false },
        { MediaGraphRuntimeState::Empty, Operation::StartThreaded, false },
        { MediaGraphRuntimeState::Empty, Operation::Stop, false },
        { MediaGraphRuntimeState::Compiled, Operation::Compile, true },
        { MediaGraphRuntimeState::Compiled, Operation::Run, true },
        { MediaGraphRuntimeState::Compiled, Operation::StartThreaded, true },
        { MediaGraphRuntimeState::Compiled, Operation::Stop, false },
        { MediaGraphRuntimeState::ThreadedRunning, Operation::Compile, false },
        { MediaGraphRuntimeState::ThreadedRunning, Operation::Run, false },
        { MediaGraphRuntimeState::ThreadedRunning, Operation::StartThreaded, false },
        { MediaGraphRuntimeState::ThreadedRunning, Operation::Stop, true },
        { MediaGraphRuntimeState::Stopped, Operation::Compile, true },
        { MediaGraphRuntimeState::Stopped, Operation::Run, false },
        { MediaGraphRuntimeState::Stopped, Operation::StartThreaded, false },
        { MediaGraphRuntimeState::Stopped, Operation::Stop, false },
        { MediaGraphRuntimeState::Aborted, Operation::Compile, true },
        { MediaGraphRuntimeState::Aborted, Operation::Run, false },
        { MediaGraphRuntimeState::Aborted, Operation::StartThreaded, false },
        { MediaGraphRuntimeState::Aborted, Operation::Stop, false },
    };
    for (const Case& entry : cases) {
        MediaGraphRuntime runtime;
        if (entry.state != MediaGraphRuntimeState::Empty) EXPECT_TRUE(ctx, runtime.compile(MediaGraph{}));
        if (entry.state == MediaGraphRuntimeState::ThreadedRunning) EXPECT_TRUE(ctx, runtime.startThreaded());
        if (entry.state == MediaGraphRuntimeState::Stopped) {
            EXPECT_TRUE(ctx, runtime.startThreaded());
            EXPECT_TRUE(ctx, runtime.stop());
        }
        if (entry.state == MediaGraphRuntimeState::Aborted) runtime.abort();
        bool result = false;
        switch (entry.operation) {
        case Operation::Compile: result = static_cast<bool>(runtime.compile(MediaGraph{})); break;
        case Operation::Run: result = static_cast<bool>(runtime.run()); break;
        case Operation::StartThreaded: result = static_cast<bool>(runtime.startThreaded()); break;
        case Operation::Stop: result = static_cast<bool>(runtime.stop()); break;
        }
        EXPECT_EQ(ctx, result, entry.allowed);
        if (runtime.threadedRunning()) EXPECT_TRUE(ctx, runtime.stop());
    }

    const MediaGraphRuntimeState resetStates[] = {
        MediaGraphRuntimeState::Empty, MediaGraphRuntimeState::Compiled,
        MediaGraphRuntimeState::ThreadedRunning, MediaGraphRuntimeState::Stopped,
        MediaGraphRuntimeState::Aborted
    };
    for (const auto state : resetStates) {
        MediaGraphRuntime runtime;
        if (state != MediaGraphRuntimeState::Empty) EXPECT_TRUE(ctx, runtime.compile(MediaGraph{}));
        if (state == MediaGraphRuntimeState::ThreadedRunning || state == MediaGraphRuntimeState::Stopped) {
            EXPECT_TRUE(ctx, runtime.startThreaded());
        }
        if (state == MediaGraphRuntimeState::Stopped) EXPECT_TRUE(ctx, runtime.stop());
        if (state == MediaGraphRuntimeState::Aborted) runtime.abort();
        runtime.reset();
        EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Empty);
    }

    for (const auto state : resetStates) {
        MediaGraphRuntime runtime;
        if (state != MediaGraphRuntimeState::Empty) EXPECT_TRUE(ctx, runtime.compile(MediaGraph{}));
        if (state == MediaGraphRuntimeState::ThreadedRunning || state == MediaGraphRuntimeState::Stopped) {
            EXPECT_TRUE(ctx, runtime.startThreaded());
        }
        if (state == MediaGraphRuntimeState::Stopped) EXPECT_TRUE(ctx, runtime.stop());
        if (state == MediaGraphRuntimeState::Aborted) runtime.abort();
        runtime.abort();
        EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Aborted);
        runtime.reset();
    }
}

void testRunningStateLifecycleBoundaries(TestContext& ctx)
{
    enum class Operation { Compile, Run, StartThreaded, Stop };
    const Operation operations[] = { Operation::Compile, Operation::Run, Operation::StartThreaded, Operation::Stop };
    const bool expected[] = { false, false, false, true };
    for (std::size_t index = 0; index < std::size(operations); ++index) {
        MediaGraph graph;
        const MediaNodeId nodeId = graph.addNode(MediaNodeKind::PacketMerge, "running.state.operation");
        MediaGraphRuntime runtime;
        EXPECT_TRUE(ctx, runtime.compile(std::move(graph)));
        auto operation = [&runtime, kind = operations[index]] {
            switch (kind) {
            case Operation::Compile: return static_cast<bool>(runtime.compile(MediaGraph{}));
            case Operation::Run: return static_cast<bool>(runtime.run());
            case Operation::StartThreaded: return static_cast<bool>(runtime.startThreaded());
            case Operation::Stop: return static_cast<bool>(runtime.stop());
            }
            return false;
        };
        auto node = std::make_unique<RunningStateOperationNode>(nodeId, operation);
        RunningStateOperationNode* view = node.get();
        EXPECT_TRUE(ctx, runtime.registerRuntimeNode(std::move(node)));
        (void)runtime.run();
        EXPECT_EQ(ctx, view->result, expected[index]);
        runtime.reset();
    }
}

void testRuntimeReportCarriesExternalAcceptanceSnapshot(TestContext& ctx)
{
    MediaGraphRuntime runtime;
    runtime.acceptanceCollector().sample(4);
    runtime.acceptanceCollector().recordError();
    const MediaGraphRuntimeReport report = MediaGraphRuntimeReporter::capture(runtime);
    EXPECT_TRUE(ctx, report.metrics.processThreadCount > 0);
    EXPECT_TRUE(ctx, report.metrics.workingSetBytes > 0);
    EXPECT_EQ(ctx, report.metrics.errorCount, static_cast<std::uint64_t>(1));
}

void testRuntimeReportKeepsGraphQueueHighWatermarkAcrossCaptures(TestContext& ctx)
{
    MediaGraphRuntime runtime;
    EXPECT_EQ(ctx, runtime.observeQueueHighWatermark(11), static_cast<std::size_t>(11));
    EXPECT_EQ(ctx, runtime.observeQueueHighWatermark(3), static_cast<std::size_t>(11));
    const MediaGraphRuntimeReport report = MediaGraphRuntimeReporter::capture(runtime);
    EXPECT_EQ(ctx, report.metrics.queuedBuffers, static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, report.metrics.peakQueuedBuffers, static_cast<std::size_t>(11));
    runtime.reset();
    EXPECT_EQ(ctx, MediaGraphRuntimeReporter::capture(runtime).metrics.peakQueuedBuffers,
              static_cast<std::size_t>(0));
}

void testWorkerFailurePropagatesIntoRuntimeReportAndResetClearsIt(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId nodeId = graph.addNode(MediaNodeKind::PacketMerge, "fault.runtime");
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(std::move(graph)));
    EXPECT_TRUE(ctx, runtime.registerRuntimeNode(std::make_unique<DeterministicRuntimeFaultNode>(
        nodeId, std::initializer_list<RuntimeFaultStep>{ RuntimeFaultStep::Failure })));
    EXPECT_TRUE(ctx, runtime.startThreaded());
    EXPECT_TRUE(ctx, waitUntil([&] { return runtime.threadedExecutor().metrics().workerErrors == 1; }));
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::ThreadedRunning);
    EXPECT_FALSE(ctx, runtime.synchronizeThreadedState());
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Aborted);
    EXPECT_EQ(ctx, runtime.threadedExecutor().state(), MediaGraphThreadedExecutorState::Aborted);
    EXPECT_FALSE(ctx, runtime.compiled());
    EXPECT_FALSE(ctx, runtime.stop());
    EXPECT_FALSE(ctx, runtime.threadedRunning());
    const MediaGraphRuntimeReport failedReport = MediaGraphRuntimeReporter::capture(runtime);
    EXPECT_EQ(ctx, failedReport.metrics.workerErrors, static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, failedReport.metrics.errorCount, static_cast<std::uint64_t>(1));
    runtime.reset();
    const MediaGraphRuntimeReport resetReport = MediaGraphRuntimeReporter::capture(runtime);
    EXPECT_EQ(ctx, resetReport.metrics.workerErrors, static_cast<std::uint64_t>(0));
    EXPECT_EQ(ctx, resetReport.metrics.errorCount, static_cast<std::uint64_t>(0));
}

} // namespace

void runEventRuntimeThreadingQueueTests(media_transcode::test::TestContext& ctx)
{
    testWaitingWorkerBlocksUntilDirectedWakeup(ctx);
    testFinishedAndErrorWorkerMetrics(ctx);
    testScriptedWaitingWakeProgressTerminalSequences(ctx);
    testWakeupBetweenSnapshotAndWaitingIsNotLost(ctx);
    testExecutorConstructsAllWorkersBeforeConcurrentStart(ctx);
    testChannelsWakeOnlyTheirConsumersAndAnyInputWakes(ctx);
    testSpscAbortReleasesBlockedProducerAndConsumer(ctx);
    testSpscCloseDeterministicallyReleasesBlockedEndpoints(ctx);
    testRuntimeMetricsCarriesAcceptanceSamples(ctx);
    testAcceptanceCollectorUsesInjectedClockForFiveSecondStall(ctx);
    testAcceptanceCollectorExcludesBaselineAndPropagatesPlatformFailure(ctx);
    testRuntimeLifecycleTransitionMatrix(ctx);
    testRunningStateLifecycleBoundaries(ctx);
    testRuntimeReportCarriesExternalAcceptanceSnapshot(ctx);
    testRuntimeReportKeepsGraphQueueHighWatermarkAcrossCaptures(ctx);
    testWorkerFailurePropagatesIntoRuntimeReportAndResetClearsIt(ctx);
}

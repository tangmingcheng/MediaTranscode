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

void testInputStreamSnapshotOwnsDeepCopyAfterSourceRelease(TestContext& ctx)
{
    ::media::ffmpeg::InputFormatContextPtr source(avformat_alloc_context());
    EXPECT_TRUE(ctx, source != nullptr);
    if (!source) return;
    AVStream* stream = avformat_new_stream(source.get(), nullptr);
    EXPECT_TRUE(ctx, stream != nullptr && stream->codecpar != nullptr);
    if (!stream || !stream->codecpar) return;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = AV_CODEC_ID_H264;
    stream->codecpar->width = 1920;
    stream->codecpar->height = 1080;
    stream->time_base = AVRational{1, 90000};
    stream->avg_frame_rate = AVRational{30, 1};
    stream->codecpar->extradata_size = 4;
    stream->codecpar->extradata = static_cast<std::uint8_t*>(av_mallocz(4 + AV_INPUT_BUFFER_PADDING_SIZE));
    EXPECT_TRUE(ctx, stream->codecpar->extradata != nullptr);
    if (!stream->codecpar->extradata) return;
    stream->codecpar->extradata[0] = 0x11;

    auto wrapped = FFmpegBufferFactory::wrapInputFormatContext(std::move(source));
    EXPECT_TRUE(ctx, wrapped);
    if (!wrapped) return;
    auto* buffer = dynamic_cast<FFmpegFormatContextBuffer*>(wrapped.value().get());
    EXPECT_TRUE(ctx, buffer && buffer->inputSnapshotComplete());
    if (!buffer) return;
    const auto* snapshot = buffer->inputStreamSnapshot(0);
    EXPECT_TRUE(ctx, snapshot && snapshot->codecParameters);
    if (!snapshot || !snapshot->codecParameters) return;
    AVFormatContext* mutableSource = buffer->context();
    mutableSource->streams[0]->codecpar->width = 320;
    mutableSource->streams[0]->codecpar->extradata[0] = 0x7f;
    auto exclusiveSource = buffer->takeInputContext();
    EXPECT_EQ(ctx, buffer->ownership(), FFmpegFormatContextOwnership::Transferred);
    EXPECT_TRUE(ctx, buffer->context() == nullptr);
    exclusiveSource.reset();
    EXPECT_EQ(ctx, snapshot->codecParameters->width, 1920);
    EXPECT_EQ(ctx, snapshot->codecParameters->extradata[0], static_cast<std::uint8_t>(0x11));
    EXPECT_EQ(ctx, snapshot->format.video.size.width, 1920);
    EXPECT_EQ(ctx, snapshot->time.timeBase.den, 90000);
}


void testInputSnapshotCreationPropagatesInvalidStreamFailures(TestContext& ctx)
{
    ::media::ffmpeg::InputFormatContextPtr nullStreamContext(avformat_alloc_context());
    EXPECT_TRUE(ctx, nullStreamContext != nullptr);
    if (!nullStreamContext) return;
    AVStream* detached = avformat_new_stream(nullStreamContext.get(), nullptr);
    EXPECT_TRUE(ctx, detached != nullptr);
    if (!detached) return;
    nullStreamContext->streams[0] = nullptr;
    auto nullStream = FFmpegBufferFactory::wrapInputFormatContext(std::move(nullStreamContext));
    EXPECT_FALSE(ctx, nullStream);
    if (!nullStream) {
        EXPECT_TRUE(ctx, nullStream.error().describe().find("stream 0 is null") != std::string::npos);
    }
    avcodec_parameters_free(&detached->codecpar);
    av_free(detached);

    ::media::ffmpeg::InputFormatContextPtr nullCodecContext(avformat_alloc_context());
    EXPECT_TRUE(ctx, nullCodecContext != nullptr);
    if (!nullCodecContext) return;
    AVStream* noCodec = avformat_new_stream(nullCodecContext.get(), nullptr);
    EXPECT_TRUE(ctx, noCodec != nullptr);
    if (!noCodec) return;
    avcodec_parameters_free(&noCodec->codecpar);
    auto nullCodec = FFmpegBufferFactory::wrapInputFormatContext(std::move(nullCodecContext));
    EXPECT_FALSE(ctx, nullCodec);
    if (!nullCodec) {
        EXPECT_TRUE(ctx, nullCodec.error().describe().find("stream 0 codec parameters are null") != std::string::npos);
    }
}

void testDemuxSameInstanceReleasesAndRebindsInputContext(TestContext& ctx)
{
    DemuxNode* demuxPtr = nullptr;
    MediaNodeId demuxId;
    auto run = [&](bool abortRun) {
        MediaGraph graph;
        const MediaNodeId source = graph.addNode(MediaNodeKind::FileInput, "demux.lifecycle.source");
        demuxId = graph.addNode(MediaNodeKind::Demux, "demux.lifecycle");
        const MediaNodeId sink = graph.addNode(MediaNodeKind::PacketMerge, "demux.lifecycle.sink");
        graph.addOutputPort(source, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true);
        graph.addInputPort(demuxId, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true);
        graph.addOutputPort(demuxId, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
        graph.addOutputPort(demuxId, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
        graph.addInputPort(sink, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
        graph.addInputPort(sink, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
        graph.connect(source, "format", demuxId, "format", "demux lifecycle", MediaGraphBuildSupport::blockingQueuePolicy(2));
        graph.connect(demuxId, "video", sink, "video", "demux video", MediaGraphBuildSupport::blockingQueuePolicy(2));
        graph.connect(demuxId, "audio", sink, "audio", "demux audio", MediaGraphBuildSupport::blockingQueuePolicy(2));
        MediaGraphExecutionContext execution;
        EXPECT_TRUE(ctx, execution.compile(graph));

        AVFormatContext* raw = nullptr;
        const auto mediaPath = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
                               "out/build/x64-debug/test.mp4";
        EXPECT_TRUE(ctx, avformat_open_input(&raw, mediaPath.string().c_str(), nullptr, nullptr) >= 0);
        if (!raw) return;
        ::media::ffmpeg::InputFormatContextPtr input(raw);
        EXPECT_TRUE(ctx, avformat_find_stream_info(input.get(), nullptr) >= 0);
        auto wrapped = FFmpegBufferFactory::wrapInputFormatContext(std::move(input));
        EXPECT_TRUE(ctx, wrapped);
        if (!wrapped) return;
        auto* format = dynamic_cast<FFmpegFormatContextBuffer*>(wrapped.value().get());
        EXPECT_TRUE(ctx, execution.findInputChannel(demuxId, "format")->push(wrapped.value()));

        if (!demuxPtr) demuxPtr = new DemuxNode(demuxId);
        EXPECT_TRUE(ctx, demuxPtr->start(execution));
        auto processed = demuxPtr->process(execution);
        EXPECT_TRUE(ctx, processed);
        EXPECT_TRUE(ctx, demuxPtr->hasBoundFormatContext());
        EXPECT_TRUE(ctx, format && format->ownership() == FFmpegFormatContextOwnership::Transferred);
        EXPECT_TRUE(ctx, format && format->context() == nullptr);
        if (abortRun) demuxPtr->abort(execution);
        else EXPECT_TRUE(ctx, demuxPtr->stop(execution));
        EXPECT_FALSE(ctx, demuxPtr->hasBoundFormatContext());
    };
    run(false);
    run(true);
    delete demuxPtr;
}

void testInputMetadataConsumersDoNotReadRuntimeStreams(TestContext& ctx)
{
    for (const char* path : {
             "src/internal/graph/nodes/packet/PacketNormalizeNode.cpp",
             "src/internal/graph/nodes/packet/PacketSourceConfigNode.cpp",
             "src/internal/graph/nodes/metadata/CodecResolverNode.cpp",
             "src/internal/graph/nodes/audio/AudioCodecResolverNode.cpp"}) {
        const std::string source = readSource(path);
        EXPECT_FALSE(ctx, source.empty());
        EXPECT_TRUE(ctx, source.find("->streams[") == std::string::npos);
        EXPECT_TRUE(ctx, source.find("AVStream") == std::string::npos);
        EXPECT_TRUE(ctx, source.find("formatBuffer->context()") == std::string::npos);
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
    MediaGraphWorker worker(merge, execution);
    EXPECT_TRUE(ctx, worker.start());
    auto eof = makePacketBuffer(false, 1);
    auto tail = makePacketBuffer(false, 2);
    EXPECT_TRUE(ctx, eof && tail);
    if (!eof || !tail) return;
    eof.value()->addFlags(MediaBufferFlag::Eof);
    EXPECT_TRUE(ctx, execution.findInputChannel(mergeId, "a")->push(eof.value()));
    EXPECT_TRUE(ctx, execution.findInputChannel(mergeId, "a")->push(eof.value()));
    EXPECT_TRUE(ctx, execution.findInputChannel(mergeId, "b")->push(tail.value()));
    EXPECT_TRUE(ctx, execution.findInputChannel(mergeId, "b")->push(eof.value()));
    worker.join();
    MediaChannel* output = execution.findInputChannel(sink, "packet");
    MediaBufferRef first;
    MediaBufferRef second;
    EXPECT_TRUE(ctx, output && output->tryPop(first));
    EXPECT_TRUE(ctx, first && !first->isEof() && first->pts() == 2);
    EXPECT_TRUE(ctx, output && output->tryPop(second));
    EXPECT_TRUE(ctx, second && second->isEof());
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

int main()
{
    TestContext ctx;
    testWaitingWorkerBlocksUntilDirectedWakeup(ctx);
    testFinishedAndErrorWorkerMetrics(ctx);
    testWakeupBetweenSnapshotAndWaitingIsNotLost(ctx);
    testExecutorConstructsAllWorkersBeforeConcurrentStart(ctx);
    testInputStreamSnapshotOwnsDeepCopyAfterSourceRelease(ctx);
    testInputSnapshotCreationPropagatesInvalidStreamFailures(ctx);
    testDemuxSameInstanceReleasesAndRebindsInputContext(ctx);
    testInputMetadataConsumersDoNotReadRuntimeStreams(ctx);
    testChannelsWakeOnlyTheirConsumersAndAnyInputWakes(ctx);
    testSpscAbortReleasesBlockedProducerAndConsumer(ctx);
    testTerminalAndMuxCompletionState(ctx);
    testPacketMergeWaitsForEveryInputTerminal(ctx);
    testPacketMergeSameInstanceRestartsWithoutTerminalState(ctx);
    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " event runtime expectation(s) failed\n";
        return 1;
    }
    std::cout << "event runtime tests passed\n";
    return 0;
}

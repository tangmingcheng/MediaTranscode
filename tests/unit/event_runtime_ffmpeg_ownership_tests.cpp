#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"
#include "internal/graph/nodes/merge/PacketMergeNode.h"
#include "internal/graph/nodes/demux/DemuxNode.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoFrameRateNode.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCodecApi.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoEncoderCodecApi.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <type_traits>

namespace media::ffmpeg::graph {

struct VideoDecodeNodeLifecycleTestAccess {
    static bool bindCodec(VideoDecodeNode& node, const MediaBufferRef& buffer)
    {
        return node.tryBindCodecContext(buffer);
    }

    static void injectInterruptedTerminal(VideoDecodeNode& node, const MediaBufferRef& terminal)
    {
        node.m_lineageState->receivePending = true;
        node.m_lineageState->flushPending = true;
        node.m_lineageState->flushIsEof = true;
        node.m_lineageState->flushSent = true;
        node.m_lineageState->flushBuffer = terminal;
        node.m_lineageState->eofEmitted = true;
        node.m_lineageState->terminals.markEof("packet");
    }

    static bool reset(const VideoDecodeNode& node)
    {
        return !node.hasCodecContext() && !node.m_lineageState->receivePending &&
               !node.m_lineageState->flushPending &&
               !node.m_lineageState->flushIsEof &&
               !node.m_lineageState->flushSent &&
               !node.m_lineageState->flushBuffer &&
               !node.m_lineageState->eofEmitted &&
               !node.m_lineageState->terminals.finished();
    }
};

struct VideoFilterNodeLifecycleTestAccess {
    static ::media::Status emit(VideoFilterNode& node,
                                MediaGraphExecutionContext& context,
                                const MediaBufferRef& buffer)
    {
        return node.emitOutput(context, "frame", buffer);
    }

    static void injectInterruptedTerminal(VideoFilterNode& node, const MediaBufferRef& terminal)
    {
        node.m_lineageState->terminalBuffer = terminal;
        node.m_lineageState->terminalPending = true;
        node.m_lineageState->terminalIsEof = true;
        node.m_lineageState->flushed = true;
        node.m_lineageState->eofEmitted = true;
        node.m_lineageState->terminals.markEof("frame");
    }

    static bool reset(const VideoFilterNode& node)
    {
        return !node.m_lineageState->terminalBuffer &&
               !node.m_lineageState->terminalPending &&
               !node.m_lineageState->terminalIsEof &&
               !node.m_lineageState->flushed &&
               !node.m_lineageState->eofEmitted &&
               !node.m_lineageState->terminals.finished();
    }

    static ::media::Status retainPrepared(
        VideoFilterNode& node,
        MediaBufferRef buffer,
        std::uint64_t generation,
        std::uint64_t releaseIdentity)
    {
        return node.retainPreparedOutput(
            std::move(buffer), generation, releaseIdentity);
    }

    static bool hasPrepared(const VideoFilterNode& node)
    {
        return static_cast<bool>(node.m_preparedOutput) ||
            node.m_preparedReservation.has_value();
    }
};

struct VideoFrameRateNodeLifecycleTestAccess {
    static void queuePending(VideoFrameRateNode& node, const MediaBufferRef& buffer)
    {
        auto guard = node.m_state->lock();
        node.m_state->data().pendingFrames.push_back({buffer, 0});
    }

    static void injectInterruptedPending(VideoFrameRateNode& node, const MediaBufferRef& buffer)
    {
        auto guard = node.m_state->lock();
        auto& state = node.m_state->data();
        state.pendingFrames.push_back({buffer, 0});
        state.lastInputFrame = {buffer, 0};
        state.terminalBuffer = buffer;
        state.terminalPending = true;
        state.terminalIsEof = true;
        state.initialized = true;
        state.started = true;
        state.flushed = true;
        state.eofEmitted = true;
        state.terminals.markEof("frame");
    }

    static bool reset(const VideoFrameRateNode& node)
    {
        auto guard = node.m_state->lock();
        const auto& state = node.m_state->data();
        return state.pendingFrames.empty() && !state.lastInputFrame.buffer &&
               !state.terminalBuffer && !state.terminalPending &&
               !state.terminalIsEof && !state.initialized && !state.started &&
               !state.flushed && !state.eofEmitted && !state.terminals.finished();
    }

    static std::size_t pending(const VideoFrameRateNode& node)
    {
        auto guard = node.m_state->lock();
        return node.m_state->data().pendingFrames.size() + node.pendingOutputBufferCount();
    }
};

} // namespace media::ffmpeg::graph

namespace {

template <typename T>
concept ExposesCodecParametersMember = requires(const T& value) {
    value.codecParameters;
};

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

::media::Result<MediaBufferRef> makeVideoFrameBuffer(std::int64_t pts)
{
    auto frame = ::media::ffmpeg::makeFrame();
    if (!frame) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("test frame allocation failed"));
    }
    frame->pts = pts;
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 16;
    frame->height = 16;
    auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video);
    if (buffer) {
        MediaTimeDescriptor time;
        time.timeBase = MediaRational{1, 25};
        buffer.value()->setTimeDescriptor(time);
    }
    return buffer;
}

::media::Result<MediaBufferRef> makeCanonicalVideoFrameBuffer(
    std::int64_t pts,
    std::uint64_t generation)
{
    auto frame = makeVideoFrameBuffer(pts);
    if (!frame) return frame;
    const auto running = MediaRunningTime::fromNanoseconds(pts * 40'000'000);
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            running, running, MediaRunningTime::fromNanoseconds(40'000'000),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            "frame-rate-node-test", MediaSourceAccessUnitSequence(pts + 1),
            MediaTimeMappingConfidence::Locked, generation});
    auto canonical = MediaCanonicalVideoFrameBuffer::create(
        frame.value(), std::move(lineage));
    if (canonical) {
        canonical.value()->setTimeDescriptor(frame.value()->timeDescriptor());
    }
    return canonical
        ? ::media::Result<MediaBufferRef>::success(std::move(canonical).value())
        : ::media::Result<MediaBufferRef>::failure(canonical.error());
}

std::string readSource(const char* path)
{
    const std::filesystem::path repository = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    std::ifstream input(repository / path, std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

void testInputStreamSnapshotOwnsDeepCopyAfterSourceRelease(TestContext& ctx)
{
    static_assert(!ExposesCodecParametersMember<FFmpegInputStreamSnapshot>,
                  "input stream snapshots must not expose owned FFmpeg storage");

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
    stream->avg_frame_rate = AVRational{0, 1};
    stream->r_frame_rate = AVRational{25, 1};
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
    EXPECT_TRUE(ctx, snapshot != nullptr);
    if (!snapshot) return;
    AVFormatContext* mutableSource = buffer->context();
    mutableSource->streams[0]->codecpar->width = 320;
    mutableSource->streams[0]->codecpar->extradata[0] = 0x7f;
    auto exclusiveSource = buffer->takeInputContext();
    EXPECT_EQ(ctx, buffer->ownership(), FFmpegFormatContextOwnership::Transferred);
    EXPECT_TRUE(ctx, buffer->context() == nullptr);
    exclusiveSource.reset();
    auto firstClone = snapshot->cloneCodecParameters();
    auto secondClone = snapshot->cloneCodecParameters();
    EXPECT_TRUE(ctx, firstClone);
    EXPECT_TRUE(ctx, secondClone);
    if (!firstClone || !secondClone) return;
    EXPECT_TRUE(ctx, firstClone.value().get() != secondClone.value().get());
    EXPECT_TRUE(ctx, firstClone.value()->extradata != secondClone.value()->extradata);
    firstClone.value()->width = 640;
    firstClone.value()->extradata[0] = 0x55;
    EXPECT_EQ(ctx, secondClone.value()->width, 1920);
    EXPECT_EQ(ctx, secondClone.value()->extradata[0], static_cast<std::uint8_t>(0x11));
    EXPECT_EQ(ctx, snapshot->format.video.size.width, 1920);
    EXPECT_EQ(ctx, snapshot->time.timeBase.den, 90000);
    EXPECT_EQ(ctx, snapshot->time.frameRate.num, 25);
    EXPECT_EQ(ctx, snapshot->time.frameRate.den, 1);
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
        const auto mediaPath = std::filesystem::path(__FILE__).parent_path().parent_path() /
                               "samples/sample_h264_aac_2560x1440.mp4";
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

void testInterruptedFfmpegNodeStateDoesNotLeakAcrossSameInstanceRestart(TestContext& ctx)
{
    MediaGraphExecutionContext execution;
    auto terminal = makePacketBuffer(false, 7);
    EXPECT_TRUE(ctx, terminal);
    if (!terminal) return;

    VideoDecodeNode decode(MediaNodeId{101});
    auto codec = FFmpegBufferFactory::wrapCodecContext(::media::ffmpeg::makeCodecContext(nullptr));
    EXPECT_TRUE(ctx, codec);
    if (!codec) return;
    EXPECT_TRUE(ctx, VideoDecodeNodeLifecycleTestAccess::bindCodec(decode, codec.value()));
    VideoDecodeNodeLifecycleTestAccess::injectInterruptedTerminal(decode, terminal.value());
    EXPECT_TRUE(ctx, decode.stop(execution));
    EXPECT_TRUE(ctx, decode.start(execution));
    EXPECT_TRUE(ctx, VideoDecodeNodeLifecycleTestAccess::reset(decode));
    auto decodeResult = decode.process(execution);
    EXPECT_TRUE(ctx, !decodeResult || decodeResult.value().state != MediaNodeProcessState::Finished);

    VideoFilterNode filter(MediaNodeId{102});
    VideoFilterNodeLifecycleTestAccess::injectInterruptedTerminal(filter, terminal.value());
    EXPECT_TRUE(ctx, filter.stop(execution));
    EXPECT_TRUE(ctx, filter.start(execution));
    EXPECT_TRUE(ctx, VideoFilterNodeLifecycleTestAccess::reset(filter));
    auto filterResult = filter.process(execution);
    EXPECT_TRUE(ctx, !filterResult || filterResult.value().state != MediaNodeProcessState::Finished);

    VideoFrameRateNode frameRate(MediaNodeId{103});
    VideoFrameRateNodeLifecycleTestAccess::injectInterruptedPending(frameRate, terminal.value());
    EXPECT_TRUE(ctx, frameRate.stop(execution));
    EXPECT_TRUE(ctx, frameRate.start(execution));
    EXPECT_TRUE(ctx, VideoFrameRateNodeLifecycleTestAccess::reset(frameRate));
    auto frameRateResult = frameRate.process(execution);
    EXPECT_TRUE(ctx, !frameRateResult || frameRateResult.value().state != MediaNodeProcessState::Finished);
}

MediaGraph makeSlowVideoNodeGraph(MediaNodeKind kind,
                                  MediaNodeId& source,
                                  MediaNodeId& nodeId,
                                  MediaNodeId& sink,
                                  bool codecInput)
{
    MediaGraph graph;
    source = graph.addNode(MediaNodeKind::ControlSignal, "slow.source");
    nodeId = graph.addNode(kind, "slow.video.node");
    sink = graph.addNode(MediaNodeKind::ControlSignal, "slow.sink");
    graph.addOutputPort(source, "frame", MediaStreamKind::Video,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(nodeId, "frame", MediaStreamKind::Video,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(nodeId, "frame", MediaStreamKind::Video,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(sink, "frame", MediaStreamKind::Video,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    if (codecInput) {
        const MediaNodeId codecSource = graph.addNode(MediaNodeKind::ControlSignal, "slow.codec.source");
        graph.addOutputPort(codecSource, "codec", MediaStreamKind::Metadata,
                            MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
        graph.addInputPort(nodeId, "codec", MediaStreamKind::Metadata,
                           MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
        graph.connect(codecSource, "codec", nodeId, "codec", "slow codec metadata",
                      MediaGraphBuildSupport::blockingQueuePolicy(1));
    }
    graph.connect(source, "frame", nodeId, "frame", "slow upstream",
                  MediaGraphBuildSupport::blockingQueuePolicy(4));
    graph.connect(nodeId, "frame", sink, "frame", "slow consumer",
                  MediaGraphBuildSupport::blockingQueuePolicy(1));
    return graph;
}

void testVideoInternalPendingDrainsBeforeSustainedUpstream(TestContext& ctx)
{
    auto filler = makeVideoFrameBuffer(700);
    auto first = makeVideoFrameBuffer(701);
    auto second = makeVideoFrameBuffer(702);
    auto upstream = makeVideoFrameBuffer(799);
    EXPECT_TRUE(ctx, filler && first && second && upstream);
    if (!filler || !first || !second || !upstream) return;

    MediaNodeId source;
    MediaNodeId nodeId;
    MediaNodeId sink;
    MediaGraph fpsGraph = makeSlowVideoNodeGraph(
        VideoFrameRateNode::staticKind(), source, nodeId, sink, false);
    MediaGraphExecutionContext fpsExecution;
    EXPECT_TRUE(ctx, fpsExecution.compile(fpsGraph));
    MediaChannel* fpsInput = fpsExecution.findInputChannel(nodeId, "frame");
    MediaChannel* fpsOutput = fpsExecution.findInputChannel(sink, "frame");
    EXPECT_TRUE(ctx, fpsInput && fpsOutput);
    EXPECT_TRUE(ctx, fpsInput->push(upstream.value()));
    EXPECT_TRUE(ctx, fpsOutput->push(filler.value()));
    VideoFrameRateNode frameRate(nodeId);
    EXPECT_TRUE(ctx, frameRate.start(fpsExecution));
    VideoFrameRateNodeLifecycleTestAccess::queuePending(frameRate, first.value());
    VideoFrameRateNodeLifecycleTestAccess::queuePending(frameRate, second.value());
    auto blocked = frameRate.process(fpsExecution);
    EXPECT_TRUE(ctx, blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, VideoFrameRateNodeLifecycleTestAccess::pending(frameRate), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, fpsInput->metrics().queue.currentSize.load(), static_cast<std::size_t>(1));
    MediaBufferRef value;
    EXPECT_TRUE(ctx, fpsOutput->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 700);
    EXPECT_TRUE(ctx, frameRate.process(fpsExecution));
    EXPECT_TRUE(ctx, fpsOutput->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 701);
    EXPECT_EQ(ctx, fpsInput->metrics().queue.currentSize.load(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, VideoFrameRateNodeLifecycleTestAccess::pending(frameRate), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, frameRate.process(fpsExecution));
    EXPECT_TRUE(ctx, fpsOutput->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 702);
    EXPECT_FALSE(ctx, fpsOutput->tryPop(value));
    EXPECT_EQ(ctx, fpsInput->metrics().queue.currentSize.load(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, frameRate.stop(fpsExecution));

    MediaGraph filterGraph = makeSlowVideoNodeGraph(
        VideoFilterNode::staticKind(), source, nodeId, sink, true);
    MediaGraphExecutionContext filterExecution;
    EXPECT_TRUE(ctx, filterExecution.compile(filterGraph));
    MediaChannel* filterInput = filterExecution.findInputChannel(nodeId, "frame");
    MediaChannel* filterOutput = filterExecution.findInputChannel(sink, "frame");
    EXPECT_TRUE(ctx, filterInput && filterOutput);
    EXPECT_TRUE(ctx, filterInput->push(upstream.value()));
    EXPECT_TRUE(ctx, filterOutput->push(filler.value()));
    VideoFilterNode filter(nodeId);
    EXPECT_TRUE(ctx, filter.start(filterExecution));
    auto pendingStatus = VideoFilterNodeLifecycleTestAccess::emit(filter, filterExecution, first.value());
    EXPECT_FALSE(ctx, pendingStatus);
    EXPECT_TRUE(ctx, pendingStatus.error().code == ::media::ErrorCode::WouldBlock);
    EXPECT_TRUE(ctx, filterOutput->tryPop(value));
    EXPECT_TRUE(ctx, filter.process(filterExecution));
    EXPECT_TRUE(ctx, filterOutput->tryPop(value));
    EXPECT_TRUE(ctx, value && value->pts() == 701);
    EXPECT_EQ(ctx, filterInput->metrics().queue.currentSize.load(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, filter.stop(filterExecution));
}

void testVideoFilterRetainsFirstPreparedFrameUntilEpochActivation(
    TestContext& ctx)
{
    auto state = MediaAvStartupVideoPreparationState::create(
        MediaAvSyncGroupKey("filter-preparation"));
    EXPECT_TRUE(ctx, state);
    if (!state) return;
    auto filterCapability = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::FilterReadiness);
    auto sequencerCapability = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::SequencerActivation);
    auto extractorCapability = MediaAvStartupVideoPreparationCapability::issue(
        state.value(), MediaAvStartupVideoPreparationRole::ExtractorFeed);
    EXPECT_TRUE(ctx, filterCapability && sequencerCapability &&
                         extractorCapability);
    if (!filterCapability || !sequencerCapability || !extractorCapability)
        return;
    EXPECT_TRUE(ctx, state.value()->begin(5, 99, 1));

    MediaNodeId source;
    MediaNodeId nodeId;
    MediaNodeId sink;
    MediaGraph graph = makeSlowVideoNodeGraph(
        VideoFilterNode::staticKind(), source, nodeId, sink, true);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* output = execution.findInputChannel(sink, "frame");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;

    VideoFilterNode filter(nodeId, nullptr, std::move(filterCapability).value());
    EXPECT_TRUE(ctx, filter.start(execution));
    auto prepared = makeVideoFrameBuffer(1234);
    EXPECT_TRUE(ctx, prepared);
    if (!prepared) return;
    EXPECT_TRUE(ctx, VideoFilterNodeLifecycleTestAccess::retainPrepared(
                         filter, prepared.value(), 5, 99));
    EXPECT_TRUE(ctx, VideoFilterNodeLifecycleTestAccess::hasPrepared(filter));
    MediaBufferRef observed;
    EXPECT_FALSE(ctx, output->tryPop(observed));
    EXPECT_TRUE(ctx, filter.process(execution));
    const std::span<const MediaBufferRef> noOutputs;
    const std::array<MediaAtomicOutputBatch, 1> batches{
        MediaAtomicOutputBatch{output, noOutputs}};
    auto extractorReservation = MediaReservedOutputTransaction::reserve(
        "Video filter ownership extractor readiness", batches);
    EXPECT_TRUE(ctx, extractorReservation && extractorReservation.value());
    if (!extractorReservation || !extractorReservation.value()) return;
    EXPECT_TRUE(ctx, extractorCapability.value().registerExtractorOutputs(
                         5, 99, extractorReservation.value()->handle()));
    const MediaPlaybackEpoch anchoredEpoch{
        MediaRunningTime::fromNanoseconds(10),
        MediaRunningTime::fromNanoseconds(20), 5};
    const MediaAudioPlaybackOrigin anchoredOrigin{
        5, anchoredEpoch.sourceStart, anchoredEpoch.masterRelease,
        0, 48'000};
    EXPECT_TRUE(ctx, state.value()->publishInitialAnchor(
                         5, 99, anchoredEpoch, anchoredOrigin));
    EXPECT_TRUE(ctx, state.value()->acknowledgeExtractorReanchor(5, 99));
    EXPECT_TRUE(ctx, sequencerCapability.value().authorizeRelease(
                         5, 99, [] { return ::media::Status::success(); }));
    EXPECT_TRUE(ctx, filter.process(execution));
    EXPECT_TRUE(ctx, output->tryPop(observed));
    EXPECT_TRUE(ctx, observed == prepared.value());
    EXPECT_FALSE(ctx, VideoFilterNodeLifecycleTestAccess::hasPrepared(filter));
    EXPECT_TRUE(ctx, filter.stop(execution));
}

void testVideoFilterPreparationStateClearsOnStopAndAbort(TestContext& ctx)
{
    const auto run = [&](bool abort) {
        auto state = MediaAvStartupVideoPreparationState::create(
            MediaAvSyncGroupKey(abort ? "filter-abort" : "filter-stop"));
        EXPECT_TRUE(ctx, state);
        if (!state) return;
        auto capability = MediaAvStartupVideoPreparationCapability::issue(
            state.value(), MediaAvStartupVideoPreparationRole::FilterReadiness);
        EXPECT_TRUE(ctx, capability && state.value()->begin(3, 17, 1));
        if (!capability) return;
        MediaGraphExecutionContext execution;
        VideoFilterNode filter(
            MediaNodeId{880}, nullptr, std::move(capability).value());
        EXPECT_TRUE(ctx, filter.start(execution));
        auto prepared = makeVideoFrameBuffer(88);
        EXPECT_TRUE(ctx, prepared);
        if (!prepared) return;
        EXPECT_TRUE(ctx, VideoFilterNodeLifecycleTestAccess::retainPrepared(
                             filter, prepared.value(), 3, 17));
        if (abort) filter.abort(execution);
        else EXPECT_TRUE(ctx, filter.stop(execution));
        EXPECT_FALSE(ctx, VideoFilterNodeLifecycleTestAccess::hasPrepared(filter));
        EXPECT_EQ(ctx, state.value()->snapshot().phase,
                  MediaAvStartupVideoPreparationPhase::Cancelled);
    };
    run(false);
    run(true);
}

void testFrameRatePreservesNonZeroTimelineOrigin(TestContext& ctx)
{
    struct Case final {
        std::int64_t pts;
        int timeBaseDenominator;
        int fps;
    };
    for (const Case testCase : {
             Case{5, 25, 25},
             Case{1'751'158'773, 90'000, 30}}) {
        MediaNodeId source;
        MediaNodeId nodeId;
        MediaNodeId sink;
        MediaGraph graph = makeSlowVideoNodeGraph(
            VideoFrameRateNode::staticKind(), source, nodeId, sink, false);
        MediaNode* nodePlan = graph.findNode(nodeId);
        EXPECT_TRUE(ctx, nodePlan != nullptr);
        if (!nodePlan) return;
        nodePlan->options.set("video.fps.num", std::to_string(testCase.fps));
        nodePlan->options.set("video.fps.den", "1");

        MediaGraphExecutionContext execution;
        EXPECT_TRUE(ctx, execution.compile(graph));
        MediaChannel* input = execution.findInputChannel(nodeId, "frame");
        MediaChannel* output = execution.findInputChannel(sink, "frame");
        EXPECT_TRUE(ctx, input && output);
        if (!input || !output) return;

        auto first = makeVideoFrameBuffer(testCase.pts);
        EXPECT_TRUE(ctx, first);
        if (!first) return;
        AVFrame* firstFrame = FFmpegFrameView::writableFrame(first.value());
        EXPECT_TRUE(ctx, firstFrame != nullptr);
        if (!firstFrame) return;
        EXPECT_EQ(ctx, av_frame_get_buffer(firstFrame, 32), 0);
        MediaTimeDescriptor time;
        time.timeBase = MediaRational{1, testCase.timeBaseDenominator};
        first.value()->setTimeDescriptor(time);
        EXPECT_TRUE(ctx, input->push(first.value()));

        VideoFrameRateNode frameRate(nodeId);
        EXPECT_TRUE(ctx, frameRate.start(execution));
        EXPECT_TRUE(ctx, frameRate.process(execution));

        MediaBufferRef observed;
        EXPECT_TRUE(ctx, output->tryPop(observed));
        EXPECT_TRUE(ctx, observed && observed->pts() == testCase.pts);
        EXPECT_EQ(ctx, VideoFrameRateNodeLifecycleTestAccess::pending(frameRate),
                  static_cast<std::size_t>(0));
        EXPECT_TRUE(ctx, frameRate.stop(execution));
    }
}

void testFrameRatePurgeClearsPendingHistoryAndRestartsGeneration(TestContext& ctx)
{
    auto filler = makeVideoFrameBuffer(700);
    auto oldFrame = makeCanonicalVideoFrameBuffer(10, 51);
    auto nextFrame = makeCanonicalVideoFrameBuffer(20, 52);
    EXPECT_TRUE(ctx, filler && oldFrame && nextFrame);
    if (!filler || !oldFrame || !nextFrame) return;

    MediaNodeId source;
    MediaNodeId nodeId;
    MediaNodeId sink;
    MediaGraph graph = makeSlowVideoNodeGraph(
        VideoFrameRateNode::staticKind(), source, nodeId, sink, false);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* input = execution.findInputChannel(nodeId, "frame");
    MediaChannel* output = execution.findInputChannel(sink, "frame");
    EXPECT_TRUE(ctx, input && output);
    if (!input || !output) return;

    auto state = std::make_shared<MediaVideoFrameRateState>(true);
    VideoFrameRateNode node(nodeId, state);
    EXPECT_TRUE(ctx, node.start(execution));
    EXPECT_TRUE(ctx, output->push(filler.value()));
    EXPECT_TRUE(ctx, input->push(oldFrame.value()));
    auto blocked = node.process(execution);
    EXPECT_TRUE(ctx, blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, state->purge({51, 52, 1}));

    MediaBufferRef observed;
    EXPECT_TRUE(ctx, output->tryPop(observed));
    EXPECT_TRUE(ctx, input->push(nextFrame.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, output->tryPop(observed));
    auto lineage = FFmpegFrameView::canonicalLineage(observed);
    EXPECT_TRUE(ctx, lineage && lineage->generation == 52);
    EXPECT_TRUE(ctx, node.stop(execution));
}

void testFrameRatePurgeCancelsRetainedTerminal(TestContext& ctx)
{
    for (const bool eof : {false, true}) {
      for (const bool videoScoped : {false, true}) {
        auto blocker = makeVideoFrameBuffer(700);
        auto oldFrame = makeCanonicalVideoFrameBuffer(10, 61);
        auto nextFrame = makeCanonicalVideoFrameBuffer(20, 62);
        EXPECT_TRUE(ctx, blocker && oldFrame && nextFrame);
        if (!blocker || !oldFrame || !nextFrame) return;

        MediaNodeId source;
        MediaNodeId nodeId;
        MediaNodeId sink;
        MediaGraph graph = makeSlowVideoNodeGraph(
            VideoFrameRateNode::staticKind(), source, nodeId, sink, false);
        MediaGraphExecutionContext execution;
        EXPECT_TRUE(ctx, execution.compile(graph));
        MediaChannel* input = execution.findInputChannel(nodeId, "frame");
        MediaChannel* output = execution.findInputChannel(sink, "frame");
        EXPECT_TRUE(ctx, input && output);
        if (!input || !output) return;

        auto state = std::make_shared<MediaVideoFrameRateState>(true);
        VideoFrameRateNode node(nodeId, state);
        EXPECT_TRUE(ctx, node.start(execution));
        EXPECT_TRUE(ctx, input->push(oldFrame.value()));
        EXPECT_TRUE(ctx, node.process(execution));
        MediaBufferRef observed;
        EXPECT_TRUE(ctx, output->tryPop(observed));
        observed.reset();

        EXPECT_TRUE(ctx, output->push(blocker.value()));
        const auto terminal = makeMediaBufferRef<MediaControlBuffer>(
            eof ? MediaControlBufferKind::Eof
                : MediaControlBufferKind::Flush);
        if (videoScoped) terminal->setStreamKind(MediaStreamKind::Video);
        EXPECT_TRUE(ctx, input->push(terminal));
        auto retained = node.process(execution);
        EXPECT_TRUE(ctx, retained &&
                             retained.value().state !=
                                 MediaNodeProcessState::Finished);
        EXPECT_TRUE(ctx, node.generationPurgeTarget()->purge({61, 62, 1}));
        EXPECT_TRUE(ctx, output->tryPop(observed) &&
                             observed == blocker.value());
        observed.reset();
        EXPECT_TRUE(ctx, node.process(execution));
        EXPECT_FALSE(ctx, output->tryPop(observed));

        EXPECT_TRUE(ctx, input->push(nextFrame.value()));
        EXPECT_TRUE(ctx, node.process(execution));
        EXPECT_TRUE(ctx, output->tryPop(observed));
        const auto lineage = FFmpegFrameView::canonicalLineage(observed);
        EXPECT_TRUE(ctx, lineage && lineage->generation == 62);
        EXPECT_TRUE(ctx, node.stop(execution));
      }
    }
}

} // namespace

void runEventRuntimeFfmpegOwnershipTests(media_transcode::test::TestContext& ctx)
{
    testInputStreamSnapshotOwnsDeepCopyAfterSourceRelease(ctx);
    testInputSnapshotCreationPropagatesInvalidStreamFailures(ctx);
    testDemuxSameInstanceReleasesAndRebindsInputContext(ctx);
    testInputMetadataConsumersDoNotReadRuntimeStreams(ctx);
    testInterruptedFfmpegNodeStateDoesNotLeakAcrossSameInstanceRestart(ctx);
    testVideoInternalPendingDrainsBeforeSustainedUpstream(ctx);
    testVideoFilterRetainsFirstPreparedFrameUntilEpochActivation(ctx);
    testVideoFilterPreparationStateClearsOnStopAndAbort(ctx);
    testFrameRatePreservesNonZeroTimelineOrigin(ctx);
    testFrameRatePurgeClearsPendingHistoryAndRestartsGeneration(ctx);
    testFrameRatePurgeCancelsRetainedTerminal(ctx);
}

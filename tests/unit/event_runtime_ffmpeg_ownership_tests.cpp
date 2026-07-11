#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/mux/MediaMuxCompletionState.h"
#include "internal/graph/nodes/merge/PacketMergeNode.h"
#include "internal/graph/nodes/demux/DemuxNode.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoFrameRateNode.h"
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

namespace media::ffmpeg::graph {

struct VideoDecodeNodeLifecycleTestAccess {
    static bool bindCodec(VideoDecodeNode& node, const MediaBufferRef& buffer)
    {
        return node.tryBindCodecContext(buffer);
    }

    static void injectInterruptedTerminal(VideoDecodeNode& node, const MediaBufferRef& terminal)
    {
        node.m_receivePending = true;
        node.m_flushPending = true;
        node.m_flushIsEof = true;
        node.m_flushSent = true;
        node.m_flushBuffer = terminal;
        node.m_eofEmitted = true;
        node.m_terminals.markEof("packet");
    }

    static bool reset(const VideoDecodeNode& node)
    {
        return !node.hasCodecContext() && !node.m_receivePending && !node.m_flushPending &&
               !node.m_flushIsEof && !node.m_flushSent && !node.m_flushBuffer &&
               !node.m_eofEmitted && !node.m_terminals.finished();
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
        node.m_terminalBuffer = terminal;
        node.m_terminalPending = true;
        node.m_terminalIsEof = true;
        node.m_flushed = true;
        node.m_eofEmitted = true;
        node.m_terminals.markEof("frame");
    }

    static bool reset(const VideoFilterNode& node)
    {
        return !node.m_terminalBuffer && !node.m_terminalPending && !node.m_terminalIsEof &&
               !node.m_flushed && !node.m_eofEmitted && !node.m_terminals.finished();
    }
};

struct VideoFrameRateNodeLifecycleTestAccess {
    static void queuePending(VideoFrameRateNode& node, const MediaBufferRef& buffer)
    {
        node.m_pendingFrames.push_back(buffer);
    }

    static void injectInterruptedPending(VideoFrameRateNode& node, const MediaBufferRef& buffer)
    {
        node.m_pendingFrames.push_back(buffer);
        node.m_lastInputFrame = buffer;
        node.m_terminalBuffer = buffer;
        node.m_terminalPending = true;
        node.m_terminalIsEof = true;
        node.m_initialized = true;
        node.m_started = true;
        node.m_flushed = true;
        node.m_eofEmitted = true;
        node.m_terminals.markEof("frame");
    }

    static bool reset(const VideoFrameRateNode& node)
    {
        return node.m_pendingFrames.empty() && !node.m_lastInputFrame && !node.m_terminalBuffer &&
               !node.m_terminalPending && !node.m_terminalIsEof && !node.m_initialized &&
               !node.m_started && !node.m_flushed && !node.m_eofEmitted &&
               !node.m_terminals.finished();
    }

    static std::size_t pending(const VideoFrameRateNode& node)
    {
        return node.m_pendingFrames.size() + node.pendingOutputBufferCount();
    }
};

} // namespace media::ffmpeg::graph

namespace {

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

} // namespace

void runEventRuntimeFfmpegOwnershipTests(media_transcode::test::TestContext& ctx)
{
    testInputStreamSnapshotOwnsDeepCopyAfterSourceRelease(ctx);
    testInputSnapshotCreationPropagatesInvalidStreamFailures(ctx);
    testDemuxSameInstanceReleasesAndRebindsInputContext(ctx);
    testInputMetadataConsumersDoNotReadRuntimeStreams(ctx);
    testInterruptedFfmpegNodeStateDoesNotLeakAcrossSameInstanceRestart(ctx);
    testVideoInternalPendingDrainsBeforeSustainedUpstream(ctx);
}

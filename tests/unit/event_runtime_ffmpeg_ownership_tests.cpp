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

} // namespace

void runEventRuntimeFfmpegOwnershipTests(media_transcode::test::TestContext& ctx)
{
    testInputStreamSnapshotOwnsDeepCopyAfterSourceRelease(ctx);
    testInputSnapshotCreationPropagatesInvalidStreamFailures(ctx);
    testDemuxSameInstanceReleasesAndRebindsInputContext(ctx);
    testInputMetadataConsumersDoNotReadRuntimeStreams(ctx);
}

#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/audio/AudioEncodeNode.h"
#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"
#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavutil/error.h>
}

#include <deque>
#include <memory>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

class ScriptedAudioEncoderCodecApi final : public AudioEncoderCodecApi {
public:
    std::deque<int> sendResults;
    std::deque<int> receiveResults;
    std::vector<int> sentSamples;
    std::vector<int64_t> sentPts;

    int sendFrame(AVCodecContext*, const AVFrame* frame) noexcept override
    {
        sentSamples.push_back(frame ? frame->nb_samples : -1);
        sentPts.push_back(frame ? frame->pts : AV_NOPTS_VALUE);
        if (sendResults.empty()) return 0;
        const int result = sendResults.front();
        sendResults.pop_front();
        return result;
    }

    int receivePacket(AVCodecContext*, AVPacket* packet) noexcept override
    {
        if (receiveResults.empty()) return AVERROR(EAGAIN);
        const int result = receiveResults.front();
        receiveResults.pop_front();
        if (result == 0) {
            if (av_new_packet(packet, 1) < 0) return AVERROR(ENOMEM);
            packet->pts = 0;
            packet->dts = 0;
            packet->duration = 1024;
        }
        return result;
    }
};

::media::ffmpeg::FramePtr makeAudioFrame(int samples, int64_t pts)
{
    auto frame = ::media::ffmpeg::makeFrame();
    if (!frame) return {};
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48000;
    frame->nb_samples = samples;
    frame->pts = pts;
    av_channel_layout_default(&frame->ch_layout, 2);
    if (av_frame_get_buffer(frame.get(), 0) < 0) return {};
    return frame;
}

void testAudioEncodeFixedFrameStateMachine(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "test.codec");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::DebugDump, "test.frame");
    const MediaNodeId encoder = graph.addNode(MediaNodeKind::AudioEncode, "test.encoder");
    const MediaNodeId packetSink = graph.addNode(MediaNodeKind::DebugDump, "test.packet");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(encoder, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(encoder, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(encoder, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    graph.addInputPort(packetSink, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    graph.connect(codecSource, "codec", encoder, "codec", "test.codec", policy);
    graph.connect(frameSource, "frame", encoder, "frame", "test.frame", policy);
    graph.connect(encoder, "packet", packetSink, "packet", "test.packet", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* codecInput = execution.findInputChannel(encoder, "codec");
    MediaChannel* frameInput = execution.findInputChannel(encoder, "frame");
    MediaChannel* packetOutput = execution.findOutputChannel(encoder, "packet");
    EXPECT_TRUE(ctx, codecInput != nullptr);
    EXPECT_TRUE(ctx, frameInput != nullptr);
    EXPECT_TRUE(ctx, packetOutput != nullptr);
    if (!codecInput || !frameInput || !packetOutput) return;

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) return;
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48000;
    codec->frame_size = 1024;
    codec->time_base = AVRational{1, 48000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, codecInput->push(codecBuffer.value()));

    auto api = std::make_shared<ScriptedAudioEncoderCodecApi>();
    api->sendResults = {AVERROR(EAGAIN), 0, 0, 0};
    api->receiveResults = {AVERROR(EAGAIN), 0, AVERROR(EAGAIN), AVERROR(EAGAIN), AVERROR_EOF};
    AudioEncodeNode node(encoder, api);
    EXPECT_TRUE(ctx, node.process(execution));

    auto blockerPacket = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, blockerPacket != nullptr);
    if (!blockerPacket) return;
    EXPECT_TRUE(ctx, av_new_packet(blockerPacket.get(), 1) >= 0);
    auto blocker = FFmpegBufferFactory::wrapPacket(std::move(blockerPacket), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, blocker);
    if (!blocker) return;
    EXPECT_TRUE(ctx, packetOutput->push(blocker.value()));

    auto fullBuffer = FFmpegBufferFactory::wrapFrame(makeAudioFrame(1024, 0), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, fullBuffer);
    if (!fullBuffer) return;
    EXPECT_TRUE(ctx, frameInput->push(fullBuffer.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(2));
    if (api->sentSamples.size() >= 2 && api->sentPts.size() >= 2) {
        EXPECT_EQ(ctx, api->sentSamples[0], 1024);
        EXPECT_EQ(ctx, api->sentSamples[1], 1024);
        EXPECT_EQ(ctx, api->sentPts[0], api->sentPts[1]);
    }

    MediaBufferRef output;
    const bool blockerPopped = packetOutput->tryPop(output);
    EXPECT_TRUE(ctx, blockerPopped);
    if (blockerPopped) EXPECT_TRUE(ctx, output == blocker.value());
    EXPECT_TRUE(ctx, node.process(execution));
    const bool packetPopped = packetOutput->tryPop(output);
    EXPECT_TRUE(ctx, packetPopped);
    if (packetPopped) EXPECT_FALSE(ctx, output->isEof());
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(2));

    auto tailBuffer = FFmpegBufferFactory::wrapFrame(makeAudioFrame(100, 1024), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, tailBuffer);
    if (!tailBuffer) return;
    EXPECT_TRUE(ctx, frameInput->push(tailBuffer.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(2));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    EXPECT_TRUE(ctx, frameInput->push(eof.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(4));
    if (api->sentSamples.size() >= 4) {
        EXPECT_EQ(ctx, api->sentSamples[2], 100);
        EXPECT_EQ(ctx, api->sentSamples[3], -1);
    }
    const bool eofPopped = packetOutput->tryPop(output);
    EXPECT_TRUE(ctx, eofPopped);
    if (eofPopped) EXPECT_TRUE(ctx, output->isEof());
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(4));
    EXPECT_FALSE(ctx, packetOutput->tryPop(output));
}

} // namespace

int main()
{
    TestContext ctx;
    RtpMuxStateMachine state;
    EXPECT_TRUE(ctx, state.bindExpectations(true, false, true, 0));
    EXPECT_TRUE(ctx, state.bindOutput());
    EXPECT_TRUE(ctx, state.markHeaderWritten());
    state.setExpectedInputs({"video"}, {"packet"});
    EXPECT_TRUE(ctx, state.markConfigReady("video"));
    EXPECT_FALSE(ctx, state.markTrailerWritten());
    EXPECT_TRUE(ctx, state.markInputEof("packet"));
    EXPECT_TRUE(ctx, state.markTrailerWritten());
    EXPECT_TRUE(ctx, state.finished());

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) return 1;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48000;
    codec->frame_size = 1024;
    av_channel_layout_default(&codec->ch_layout, 2);

    AudioEncoderFrameQueue frameQueue;
    EXPECT_TRUE(ctx, frameQueue.configure(*codec));

    auto makeFrame = [](int samples, int64_t pts) {
        auto frame = ::media::ffmpeg::makeFrame();
        frame->format = AV_SAMPLE_FMT_FLTP;
        frame->sample_rate = 48000;
        frame->nb_samples = samples;
        frame->pts = pts;
        av_channel_layout_default(&frame->ch_layout, 2);
        if (av_frame_get_buffer(frame.get(), 0) < 0) {
            frame.reset();
        }
        return frame;
    };

    auto firstInput = makeFrame(1098, 0);
    EXPECT_TRUE(ctx, firstInput != nullptr);
    if (!firstInput) return 1;
    EXPECT_TRUE(ctx, frameQueue.push(*firstInput));
    EXPECT_TRUE(ctx, frameQueue.hasFullFrame());
    auto firstOutput = frameQueue.popFullFrame();
    EXPECT_TRUE(ctx, firstOutput);
    if (firstOutput) {
        EXPECT_EQ(ctx, firstOutput.value()->nb_samples, 1024);
        EXPECT_EQ(ctx, firstOutput.value()->pts, static_cast<int64_t>(0));
    }
    EXPECT_FALSE(ctx, frameQueue.hasFullFrame());
    EXPECT_EQ(ctx, frameQueue.queuedSamples(), 74);

    auto secondInput = makeFrame(950, 1098);
    EXPECT_TRUE(ctx, secondInput != nullptr);
    if (!secondInput) return 1;
    EXPECT_TRUE(ctx, frameQueue.push(*secondInput));
    EXPECT_TRUE(ctx, frameQueue.hasFullFrame());
    auto secondOutput = frameQueue.popFullFrame();
    EXPECT_TRUE(ctx, secondOutput);
    if (secondOutput) {
        EXPECT_EQ(ctx, secondOutput.value()->nb_samples, 1024);
        EXPECT_EQ(ctx, secondOutput.value()->pts, static_cast<int64_t>(1024));
    }
    EXPECT_EQ(ctx, frameQueue.queuedSamples(), 0);

    auto tailInput = makeFrame(100, 2048);
    EXPECT_TRUE(ctx, tailInput != nullptr);
    if (!tailInput) return 1;
    EXPECT_TRUE(ctx, frameQueue.push(*tailInput));
    auto tailOutput = frameQueue.popRemainingFrame();
    EXPECT_TRUE(ctx, tailOutput);
    if (tailOutput) {
        EXPECT_EQ(ctx, tailOutput.value()->nb_samples, 100);
        EXPECT_EQ(ctx, tailOutput.value()->pts, static_cast<int64_t>(2048));
    }
    EXPECT_EQ(ctx, frameQueue.queuedSamples(), 0);

    AudioEncoderFrameQueue overlapQueue;
    EXPECT_TRUE(ctx, overlapQueue.configure(*codec));
    auto overlapFirst = makeFrame(512, 0);
    auto overlapSecond = makeFrame(512, 256);
    EXPECT_TRUE(ctx, overlapFirst != nullptr);
    EXPECT_TRUE(ctx, overlapSecond != nullptr);
    if (overlapFirst && overlapSecond) {
        EXPECT_TRUE(ctx, overlapQueue.push(*overlapFirst));
        const auto overlapStatus = overlapQueue.push(*overlapSecond);
        EXPECT_FALSE(ctx, overlapStatus);
        if (!overlapStatus) EXPECT_EQ(ctx, overlapStatus.error().code, media::ErrorCode::InvalidArgument);
    }

    AudioEncoderFrameQueue gapQueue;
    EXPECT_TRUE(ctx, gapQueue.configure(*codec));
    auto gapFirst = makeFrame(512, 0);
    auto gapSecond = makeFrame(512, 768);
    EXPECT_TRUE(ctx, gapFirst != nullptr);
    EXPECT_TRUE(ctx, gapSecond != nullptr);
    if (gapFirst && gapSecond) {
        EXPECT_TRUE(ctx, gapQueue.push(*gapFirst));
        const auto gapStatus = gapQueue.push(*gapSecond);
        EXPECT_FALSE(ctx, gapStatus);
        if (!gapStatus) EXPECT_EQ(ctx, gapStatus.error().code, media::ErrorCode::InvalidArgument);
    }
    testAudioEncodeFixedFrameStateMachine(ctx);
    return ctx.failures == 0 ? 0 : 1;
}

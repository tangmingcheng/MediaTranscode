#include "common/TestAssert.h"

#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"
#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

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
    return ctx.failures == 0 ? 0 : 1;
}

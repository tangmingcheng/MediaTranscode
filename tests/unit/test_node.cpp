#include "common/TestAssert.h"

#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"

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
    return ctx.failures == 0 ? 0 : 1;
}

#include "common/TestAssert.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

using namespace media::ffmpeg::graph;

namespace media_transcode::test::scheduled_rtp_output {

void runScheduledRtpSenderNodeTests(TestContext& ctx);
void runDualMediaSdpPublisherTests(TestContext& ctx);
void runScheduledRtpOutputAssemblyTests(TestContext& ctx);

} // namespace media_transcode::test::scheduled_rtp_output

int main()
{
    media_transcode::test::TestContext ctx;
    MediaProtocolOutputGenerationState generationState(
        "rtp_video_output_generation_state");
    const auto canReserve = [&generationState](std::uint64_t generation) {
        return static_cast<bool>(generationState.reserveCommit(generation));
    };
    EXPECT_TRUE(ctx, generationState.permitActivatedGeneration(1));
    EXPECT_TRUE(ctx, canReserve(1));
    EXPECT_TRUE(ctx, generationState.purge(MediaAvGenerationPurge{1, 2, 1}));
    EXPECT_FALSE(ctx, canReserve(1));
    EXPECT_FALSE(ctx, canReserve(2));
    EXPECT_TRUE(ctx, generationState.permitActivatedGeneration(2));
    EXPECT_TRUE(ctx, canReserve(2));
    media_transcode::test::scheduled_rtp_output::
        runScheduledRtpSenderNodeTests(ctx);
    media_transcode::test::scheduled_rtp_output::
        runDualMediaSdpPublisherTests(ctx);
    media_transcode::test::scheduled_rtp_output::
        runScheduledRtpOutputAssemblyTests(ctx);
    return ctx.failures == 0 ? 0 : 1;
}

#include "common/TestAssert.h"

namespace media_transcode::test::scheduled_rtp_output {

void runScheduledRtpSenderNodeTests(TestContext& ctx);
void runDualMediaSdpPublisherTests(TestContext& ctx);
void runScheduledRtpOutputAssemblyTests(TestContext& ctx);

} // namespace media_transcode::test::scheduled_rtp_output

int main()
{
    media_transcode::test::TestContext ctx;
    media_transcode::test::scheduled_rtp_output::
        runScheduledRtpSenderNodeTests(ctx);
    media_transcode::test::scheduled_rtp_output::
        runDualMediaSdpPublisherTests(ctx);
    media_transcode::test::scheduled_rtp_output::
        runScheduledRtpOutputAssemblyTests(ctx);
    return ctx.failures == 0 ? 0 : 1;
}

#include "common/EventRuntimeTestCases.h"

#include <iostream>

int main()
{
    media_transcode::test::TestContext ctx;
    runEventRuntimeThreadingQueueTests(ctx);
    runEventRuntimeFfmpegOwnershipTests(ctx);
    runEventRuntimeMultiInputTests(ctx);
    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " event runtime expectation(s) failed\n";
        return 1;
    }
    std::cout << "event runtime tests passed\n";
    return 0;
}

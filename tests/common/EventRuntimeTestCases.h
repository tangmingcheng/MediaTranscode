#pragma once

#include "common/TestAssert.h"

void runEventRuntimeThreadingQueueTests(media_transcode::test::TestContext& ctx);
void runEventRuntimeFfmpegOwnershipTests(media_transcode::test::TestContext& ctx);
void runEventRuntimeMultiInputTests(media_transcode::test::TestContext& ctx);
void runAvOutputSchedulerTests(media_transcode::test::TestContext& ctx);

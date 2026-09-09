#pragma once

#include "internal/graph/time/MediaRunningTime.h"

namespace media::ffmpeg::graph {

// Ordered EOS drains accepted media; all workers must exit before retirement.
// Silence measures real payload progress, ordered EOS publication and worker
// exits, never process calls or wakeups. This is a failure-detection policy,
// not a CPU/driver execution-time guarantee or network residence budget.
// Resource ownership persists until physical asynchronous retirement completes.
struct MediaRuntimeBranchDrainPlan final {
    MediaRunningTime maximumProgressSilence;
};

} // namespace media::ffmpeg::graph

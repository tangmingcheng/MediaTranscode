#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaRuntimeReclamationMode { DedicatedSingleShotOwner };

// Structural owner/thread counts come from the segment lifecycle. Silence is
// the existing session failure-detection policy, never a driver WCET promise.
struct MediaRuntimeReclamationPlan final {
    MediaRuntimeReclamationMode mode;
    std::uint64_t ownerThreads;
    std::uint64_t commandSlots;
    std::uint64_t fixedStorageBytes;
    MediaRunningTime maximumProgressSilence;
};

} // namespace media::ffmpeg::graph

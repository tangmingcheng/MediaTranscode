#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <optional>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvSchedulerCommitKind { Video, Audio, Terminal };

struct MediaAvSchedulerPendingCommit final {
    MediaAvSchedulerCommitKind kind;
    bool terminalFinishes = false;
    std::optional<std::uint64_t> generation;
    std::optional<MediaRunningTime> scheduledAt;
};

} // namespace media::ffmpeg::graph

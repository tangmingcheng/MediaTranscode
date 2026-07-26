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
    MediaBufferRef displayedVideoClone;
    std::optional<MediaSourceAccessUnitSequence> displayedVideoSequence;
    std::optional<MediaRunningTime> displayedVideoMasterTime;
    bool terminalFinishes = false;
    std::optional<std::uint64_t> generation;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <optional>

namespace media::ffmpeg::graph {

enum class MediaAvSchedulerCommitKind { Video, Audio, Terminal };

struct MediaAvSchedulerPendingCommit final {
    MediaAvSchedulerCommitKind kind;
    MediaBufferRef displayedVideoClone;
    std::optional<MediaSourceAccessUnitSequence> displayedVideoSequence;
    std::optional<MediaRunningTime> displayedVideoMasterTime;
    bool terminalFinishes = false;
};

} // namespace media::ffmpeg::graph

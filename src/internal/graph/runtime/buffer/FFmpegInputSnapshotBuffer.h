#pragma once

#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"

namespace media::ffmpeg::graph {

class FFmpegInputSnapshotBuffer {
public:
    virtual ~FFmpegInputSnapshotBuffer() = default;
    virtual const FFmpegInputStreamSnapshot* inputStreamSnapshot(
        int streamIndex) const noexcept = 0;
    virtual bool inputSnapshotComplete() const noexcept = 0;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/planner/realtime/MediaPreparedGenericInput.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaDemuxInputBuffer.h"

namespace media::ffmpeg::graph {

class MediaPreparedGenericInputBuffer final : public MediaBuffer,
                                               public FFmpegInputSnapshotBuffer,
                                               public MediaDemuxInputBuffer {
public:
    explicit MediaPreparedGenericInputBuffer(MediaPreparedGenericInput input);

    MediaBufferType type() const noexcept override;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(int streamIndex) const noexcept override;
    bool inputSnapshotComplete() const noexcept override;
    ::media::Result<MediaDemuxInputSession> takeDemuxSession() override;

    const MediaPreparedGenericInputPlan& plan() const noexcept;
    const MediaPreparedGenericInputEvidence& evidence() const noexcept;
    const MediaAvSyncStartupPolicy& startup() const noexcept;

private:
    MediaPreparedGenericInput m_input;
    bool m_transferred = false;
};

} // namespace media::ffmpeg::graph

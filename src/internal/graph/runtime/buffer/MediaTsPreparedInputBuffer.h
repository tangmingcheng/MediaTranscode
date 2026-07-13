#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaTsPreparedInputBuffer final : public MediaBuffer,
                                         public FFmpegInputSnapshotBuffer {
public:
    static ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>> create(
        std::unique_ptr<MediaTsDemuxSession> session);

    MediaBufferType type() const noexcept override;
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(
        int streamIndex) const noexcept override;
    bool inputSnapshotComplete() const noexcept override { return true; }
    const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept;
    const MediaTsProgramInventorySnapshot& programInventory() const noexcept;
    ::media::Result<std::unique_ptr<MediaTsDemuxSession>> takeSession();

private:
    MediaTsPreparedInputBuffer(std::unique_ptr<MediaTsDemuxSession> session,
                               std::vector<FFmpegInputStreamSnapshot> streamSnapshots);
    std::unique_ptr<MediaTsDemuxSession> m_session;
    std::vector<FFmpegInputStreamSnapshot> m_streamSnapshots;
    std::vector<FFmpegInputProgramSnapshot> m_programSnapshots;
    MediaTsProgramInventorySnapshot m_programInventory;
};

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"

#include <functional>
#include <memory>

namespace media::ffmpeg::graph {

using MediaTsRuntimeSessionFactory = std::function<
    ::media::Result<std::unique_ptr<MediaTsDemuxSession>>()>;

class MediaTsPreparedInputBuffer final : public MediaBuffer,
                                         public FFmpegInputSnapshotBuffer {
public:
    static ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>> create(
        std::unique_ptr<MediaTsDemuxSession> preflightSession,
        MediaTsRuntimeSessionFactory runtimeSessionFactory);

    MediaBufferType type() const noexcept override;
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(
        int streamIndex) const noexcept override;
    bool inputSnapshotComplete() const noexcept override { return true; }
    const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept;
    const MediaTsProgramInventorySnapshot& programInventory() const noexcept;
    ::media::Status materializeSession();
    ::media::Result<std::unique_ptr<MediaTsDemuxSession>> takeSession();

private:
    MediaTsPreparedInputBuffer(
        std::vector<FFmpegInputStreamSnapshot> streamSnapshots,
        std::vector<FFmpegInputProgramSnapshot> programSnapshots,
        MediaTsProgramInventorySnapshot programInventory,
        MediaTsRuntimeSessionFactory runtimeSessionFactory);
    std::unique_ptr<MediaTsDemuxSession> m_session;
    MediaTsRuntimeSessionFactory m_runtimeSessionFactory;
    std::vector<FFmpegInputStreamSnapshot> m_streamSnapshots;
    std::vector<FFmpegInputProgramSnapshot> m_programSnapshots;
    MediaTsProgramInventorySnapshot m_programInventory;
};

} // namespace media::ffmpeg::graph

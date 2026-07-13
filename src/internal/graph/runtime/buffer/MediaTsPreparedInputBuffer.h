#pragma once

#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaTsPreparedInputBuffer final : public MediaBuffer {
public:
    static ::media::Result<std::unique_ptr<MediaTsPreparedInputBuffer>> create(
        std::unique_ptr<MediaTsInputSession> session);

    MediaBufferType type() const noexcept override;
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept;
    const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept;
    const MediaTsProgramInventorySnapshot& programInventory() const noexcept;
    ::media::Result<std::unique_ptr<MediaTsInputSession>> takeSession();

private:
    MediaTsPreparedInputBuffer(std::unique_ptr<MediaTsInputSession> session,
                               std::vector<FFmpegInputStreamSnapshot> streamSnapshots);
    std::unique_ptr<MediaTsInputSession> m_session;
    std::vector<FFmpegInputStreamSnapshot> m_streamSnapshots;
    std::vector<FFmpegInputProgramSnapshot> m_programSnapshots;
    MediaTsProgramInventorySnapshot m_programInventory;
};

} // namespace media::ffmpeg::graph

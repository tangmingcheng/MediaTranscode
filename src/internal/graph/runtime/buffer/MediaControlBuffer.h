#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

enum class MediaControlBufferKind {
    Unknown,
    Eof,
    Flush,
    Abort
};

class MediaControlBuffer final : public MediaBuffer {
public:
    explicit MediaControlBuffer(MediaControlBufferKind kind);

    MediaBufferType type() const noexcept override;
    MediaControlBufferKind controlKind() const noexcept;

private:
    MediaControlBufferKind m_kind = MediaControlBufferKind::Unknown;
};

} // namespace media::ffmpeg::graph

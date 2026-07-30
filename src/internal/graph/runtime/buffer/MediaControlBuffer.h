#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <cstdint>
#include <optional>

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
    MediaControlBuffer(
        MediaControlBufferKind kind,
        std::uint64_t generation);

    MediaBufferType type() const noexcept override;
    MediaControlBufferKind controlKind() const noexcept;
    std::optional<std::uint64_t> generation() const noexcept;

private:
    void initialize(MediaControlBufferKind kind);

    MediaControlBufferKind m_kind = MediaControlBufferKind::Unknown;
    std::optional<std::uint64_t> m_generation;
};

} // namespace media::ffmpeg::graph

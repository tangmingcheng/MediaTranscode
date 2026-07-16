#pragma once

#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class MediaControlBuffer;

enum class MediaStartupReleaseTransactionKind {
    Release,
    Control
};

class MediaStartupReleaseTransactionBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(MediaBufferRef release);
    static ::media::Result<MediaBufferRef> createControl(MediaBufferRef control);

    MediaBufferType type() const noexcept override;
    MediaStartupReleaseTransactionKind transactionKind() const noexcept;
    const MediaBufferRef& payload() const noexcept;
    const MediaAvStartupReleaseBuffer* release() const noexcept;
    const MediaControlBuffer* control() const noexcept;

private:
    MediaStartupReleaseTransactionBuffer(
        MediaStartupReleaseTransactionKind transactionKind,
        MediaBufferRef payload);
    const MediaStartupReleaseTransactionKind m_transactionKind;
    const MediaBufferRef m_payload;
};

} // namespace media::ffmpeg::graph

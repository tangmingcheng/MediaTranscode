#pragma once

#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaControlBuffer;

enum class MediaStartupReleaseTransactionKind {
    Release,
    Control
};

class MediaStartupReleaseTransactionBuffer final : public MediaBuffer {
    class ConstructionKey final {
        friend class MediaStartupReleaseTransactionBuffer;
        ConstructionKey() = default;
    };
public:
    using AllocationAllocator = MediaLineageAllocator<MediaStartupReleaseTransactionBuffer>;
    static ::media::Result<MediaBufferRef> create(
        const MediaStartupSharedObjectPlan& storage, MediaBufferRef release);
    static ::media::Result<MediaBufferRef> reanchor(
        const MediaStartupReleaseTransactionBuffer& transaction,
        MediaPlaybackActivation activation);
    static ::media::Result<MediaBufferRef> createControl(
        const MediaStartupSharedObjectPlan& storage, MediaBufferRef control);

    MediaBufferType type() const noexcept override;
    MediaStartupReleaseTransactionKind transactionKind() const noexcept;
    const MediaBufferRef& payload() const noexcept;
    const MediaAvStartupReleaseBuffer* release() const noexcept;
    const MediaControlBuffer* control() const noexcept;
    std::uint64_t releaseIdentity() const noexcept;

    MediaStartupReleaseTransactionBuffer(
        ConstructionKey,
        MediaStartupSharedObjectPlan storage,
        MediaStartupReleaseTransactionKind transactionKind,
        MediaBufferRef payload,
        std::uint64_t releaseIdentity);
private:
    static ::media::Result<MediaBufferRef> allocateRelease(
        const MediaStartupSharedObjectPlan& storage, MediaBufferRef release, std::uint64_t identity);

    const MediaStartupSharedObjectPlan m_storage;
    const MediaStartupReleaseTransactionKind m_transactionKind;
    const MediaBufferRef m_payload;
    const std::uint64_t m_releaseIdentity;
};

} // namespace media::ffmpeg::graph

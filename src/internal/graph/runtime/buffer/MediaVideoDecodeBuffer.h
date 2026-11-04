#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/lineage/MediaVideoDisplayLineage.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationAllocator.h"
#include <memory_resource>

namespace media::ffmpeg::graph {

struct MediaVideoDecodeAllocationGeometry final {
    MediaSharedAllocationGeometry accessUnit;
    MediaSharedAllocationGeometry display;
    MediaSharedAllocationGeometry wrapper;
};

// Shares one raw packet or frame. AU identity and decoded display identity are
// separate: a decoder may emit zero or several frames for an input AU.
class MediaVideoDecodeBuffer final : public MediaBuffer {
    class ConstructionKey final {
        friend class MediaVideoDecodeBuffer;
        ConstructionKey() = default;
    };
public:
    static ::media::Result<MediaVideoDecodeAllocationGeometry> allocationGeometry();
    static ::media::Result<MediaBufferRef> create(
        std::pmr::memory_resource& allocation, MediaBufferRef media, std::shared_ptr<const MediaVideoDecodeLineage> lineage,
        std::optional<std::uint64_t> decodedSequence);
    // Rebinds transformed pixels to the original display point; never projects
    // the transformed frame's timestamp as though it came from the decoder.
    static ::media::Result<MediaBufferRef> withDisplayLineage(
        std::pmr::memory_resource& allocation, MediaBufferRef media,
        std::shared_ptr<const MediaVideoDisplayLineage> display);
    const std::shared_ptr<const MediaVideoDisplayLineage>& displayLineage() const noexcept
    { return m_display; }
    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    const std::shared_ptr<const MediaVideoDecodeLineage>& lineage() const noexcept { return m_lineage; }
    std::optional<std::uint64_t> decodedSequence() const noexcept
    { return m_display ? std::optional{m_display->decodedSequence} : std::nullopt; }
    std::optional<MediaRunningTime> decodedPresentation() const noexcept
    { return m_display ? std::optional{m_display->presentation} : std::nullopt; }
    MediaVideoDecodeBuffer(ConstructionKey, MediaBufferRef media,
        std::shared_ptr<const MediaVideoDecodeLineage> lineage,
        std::shared_ptr<const MediaVideoDisplayLineage> display);
private:
    MediaBufferRef m_media;
    std::shared_ptr<const MediaVideoDecodeLineage> m_lineage;
    std::shared_ptr<const MediaVideoDisplayLineage> m_display;
};

} // namespace media::ffmpeg::graph

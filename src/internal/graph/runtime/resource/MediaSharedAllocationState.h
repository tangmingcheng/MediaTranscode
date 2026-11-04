#pragma once
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"
#include "media_transcode/Result.h"
#include <atomic>
#include <memory>
#include <new>
#include <optional>

namespace media::ffmpeg::graph {
class MediaSharedAllocationProbeComplete final : public std::bad_alloc {
public:
    explicit MediaSharedAllocationProbeComplete(const void* identity) noexcept : m_identity(identity) {}
    const void* identity() const noexcept { return m_identity; }
private:
    const void* m_identity;
};
class MediaSharedAllocationMismatch final : public std::bad_alloc {};

// Synchronous preparation-only recorder. Production allocators never own it.
class MediaSharedAllocationState final {
public:
    static ::media::Result<std::shared_ptr<MediaSharedAllocationState>> create();
    [[noreturn]] void observe(const MediaSharedAllocationGeometry& geometry);
    // Only read after the synchronous probing call has unwound.
    const std::optional<MediaSharedAllocationGeometry>& observed() const noexcept { return m_observed; }
private:
    MediaSharedAllocationState() = default;
    std::atomic<bool> m_attempted{false};
    std::optional<MediaSharedAllocationGeometry> m_observed;
};
} // namespace media::ffmpeg::graph

#pragma once
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
// Declare before the ordinary allocations it covers. Moving transfers their
// reservation; assignment could release credit before the old backing dies.
class MediaPreparationBackingOwner final {
public:
    explicit MediaPreparationBackingOwner(std::optional<MediaPreparationStorageLease> lease) noexcept
        : m_lease(std::move(lease)) {}
    MediaPreparationBackingOwner(MediaPreparationBackingOwner&&) noexcept = default;
    MediaPreparationBackingOwner& operator=(MediaPreparationBackingOwner&&) = delete;
    MediaPreparationBackingOwner(const MediaPreparationBackingOwner&) = delete;
    MediaPreparationBackingOwner& operator=(const MediaPreparationBackingOwner&) = delete;
private:
    std::optional<MediaPreparationStorageLease> m_lease;
};
} // namespace media::ffmpeg::graph

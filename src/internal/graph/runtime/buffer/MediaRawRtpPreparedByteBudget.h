#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/model/MediaRawRtpPreparationStoragePlan.h"
#include "internal/graph/model/MediaPreparedRtpStorageReservation.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaRawRtpPreparationPhase {
    Capture,
    Sealed
};

struct MediaRawRtpPreparedByteBudgetSnapshot final {
    std::size_t capacity;
    std::size_t wireCapacity;
    std::size_t observedBytes;
    std::size_t retainedBytes;
    MediaRawRtpPreparationPhase phase;
    bool probeActive;
};

class MediaRawRtpPreparedByteBudget final {
public:
    static ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>
    create(const MediaRawRtpPreparationStoragePlan& plan,
        const MediaPreparationStorageScope& scope);

    ::media::Status observe(std::size_t bytes);
    std::pmr::memory_resource& storageResource() const noexcept;
    MediaLineageAllocationScope allocationScope() const noexcept;
    const MediaPreparedRtpDatagramAllocationFacts& entryAllocationFacts() const noexcept;
    MediaPreparedRtpStorageReservation storageReservation() const;
    ::media::Status fail(::media::ErrorInfo error);
    ::media::Status sealPreflight();
    ::media::Status requireSealed() const;
    ::media::Status validate() const;
    MediaRawRtpPreparedByteBudgetSnapshot snapshot() const noexcept;

private:
    friend class MediaRawRtpProbeLease;
    ::media::Status acquireProbe();
    void releaseProbe() noexcept;
    MediaRawRtpPreparedByteBudget(MediaRawRtpPreparationStoragePlan plan,
        std::shared_ptr<MediaPreparationStorageBudget> storage,
        std::shared_ptr<MediaPreparationMemoryResource> resource) noexcept;
    ::media::Status observeLocked(std::size_t bytes);
    ::media::Status failLocked(::media::ErrorInfo error);

    const MediaRawRtpPreparationStoragePlan m_plan;
    std::shared_ptr<MediaPreparationStorageBudget> m_storage;
    std::shared_ptr<MediaPreparationMemoryResource> m_resource;
    mutable std::mutex m_mutex;
    std::size_t m_observedBytes = 0;
    MediaRawRtpPreparationPhase m_phase = MediaRawRtpPreparationPhase::Capture;
    bool m_probeActive = false;
    std::optional<::media::ErrorInfo> m_error;
};

} // namespace media::ffmpeg::graph

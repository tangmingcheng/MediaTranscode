#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/MediaPlaybackActivation.h"
#include "internal/graph/sync/MediaAvStartupReleaseKind.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"

#include "internal/graph/model/MediaStartupSharedObjectPlan.h"

#include <optional>
#include <memory_resource>

namespace media::ffmpeg::graph {

class MediaAvStartupEnvelopeBuffer final : public MediaBuffer {
    class ConstructionKey final {
        friend class MediaAvStartupEnvelopeBuffer;
        ConstructionKey() = default;
    };
public:
    using AllocationAllocator = std::pmr::polymorphic_allocator<MediaAvStartupEnvelopeBuffer>;
    static ::media::Result<MediaBufferRef> create(const MediaStartupSharedObjectPlan& storage, MediaBufferRef media,
                                                   MediaAvStartupAccessUnit unit,
                                                   MediaRunningTime observedAt);

    MediaBufferType type() const noexcept override;
    const MediaBufferRef& media() const noexcept;
    const MediaAvStartupAccessUnit& unit() const noexcept;
    MediaRunningTime observedAt() const noexcept;

    MediaAvStartupEnvelopeBuffer(ConstructionKey, MediaBufferRef media,
                                 MediaAvStartupAccessUnit unit,
                                 MediaRunningTime observedAt);
private:
    MediaBufferRef m_media;
    MediaAvStartupAccessUnit m_unit;
    MediaRunningTime m_observedAt;
};

class MediaAvStartupClockBuffer final : public MediaBuffer {
public:
    explicit MediaAvStartupClockBuffer(MediaRunningTime masterNow);
    MediaBufferType type() const noexcept override;
    MediaRunningTime masterNow() const noexcept;

private:
    MediaRunningTime m_masterNow;
};

struct MediaAvReleasedUnit final {
    MediaBufferRef media;
    std::uint32_t trimLeadingSamples;
};

using MediaAvReleasedUnits = MediaFixedCapacityStorage<MediaAvReleasedUnit>;

class MediaAvStartupReleaseBuffer final : public MediaBuffer {
    class ConstructionKey final {
        friend class MediaAvStartupReleaseBuffer;
        ConstructionKey() = default;
    };
public:
    using AllocationAllocator = MediaLineageAllocator<MediaAvStartupReleaseBuffer>;
    static ::media::Status validateReleaseKind(
        MediaAvStartupReleaseKind releaseKind) noexcept;
    static ::media::Result<MediaBufferRef> create(
        MediaStartupSharedObjectPlan storage,
        MediaAvSyncGroupKey groupKey,
        MediaAvStartupReleaseKind releaseKind,
        MediaPlaybackActivation activation,
        MediaAvReleasedUnits video,
        MediaAvReleasedUnits audio,
        std::optional<std::uint64_t> completedTransitionSequence);

    MediaBufferType type() const noexcept override;
    const MediaLineageAllocationScope& allocationScope() const noexcept { return m_storage.allocation(); }
    const MediaStartupSharedObjectPlan& storagePlan() const noexcept { return m_storage; }
    const MediaAvSyncGroupKey& groupKey() const noexcept;
    MediaAvStartupReleaseKind releaseKind() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaPlaybackActivation& activation() const noexcept;
    const MediaAvReleasedUnits& video() const noexcept;
    const MediaAvReleasedUnits& audio() const noexcept;
    const std::optional<std::uint64_t>&
    completedTransitionSequence() const noexcept;

    MediaAvStartupReleaseBuffer(ConstructionKey, MediaStartupSharedObjectPlan storage,
                                MediaAvSyncGroupKey groupKey,
                                MediaAvStartupReleaseKind releaseKind,
                                MediaPlaybackActivation activation,
                                MediaAvReleasedUnits video,
                                MediaAvReleasedUnits audio,
                                std::optional<std::uint64_t>
                                    completedTransitionSequence);
private:
    // Declared before storage so its owner outlives backing deallocation.
    MediaStartupSharedObjectPlan m_storage;
    MediaAvSyncGroupKey m_groupKey;
    MediaAvStartupReleaseKind m_releaseKind;
    MediaPlaybackActivation m_activation;
    MediaAvReleasedUnits m_video;
    MediaAvReleasedUnits m_audio;
    std::optional<std::uint64_t> m_completedTransitionSequence;
};

} // namespace media::ffmpeg::graph

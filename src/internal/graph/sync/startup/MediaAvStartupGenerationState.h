#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/resource/MediaAvStartupHashMemoryResource.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvStartupGenerationState final {
public:
    using AllocationAllocator = MediaLineageAllocator<MediaAvStartupGenerationState>;
    static ::media::Result<MediaSharedAllocationGeometry> sharedAllocationGeometry();
    MediaAvStartupGenerationState(MediaAvSyncGroupKey groupKey, MediaLineageAllocationScope allocation,
        MediaAvStartupGenerationStoragePlan storage);
    MediaAvStartupGenerationState(const MediaAvStartupGenerationState&) = delete;
    MediaAvStartupGenerationState& operator=(const MediaAvStartupGenerationState&) = delete;
    MediaAvStartupGenerationState(MediaAvStartupGenerationState&&) = delete;
    MediaAvStartupGenerationState& operator=(MediaAvStartupGenerationState&&) = delete;
    static ::media::Result<std::array<MediaAvStartupHashGeometry, 2>> allocationGeometry(
        std::size_t maximumElements);
    ::media::Status prepareStorage();

    static constexpr std::string_view plannedIdentity() noexcept
    {
        return "startup_generation_state";
    }

    ::media::Status store(const MediaAvSyncGroupKey& groupKey,
                          const MediaAvStartupAccessUnit& unit,
                          MediaBufferRef media);
    ::media::Result<MediaBufferRef> take(const MediaAvStartupUnitId& id);
    void erase(const MediaAvStartupUnitId& id) noexcept;
    void erase(const MediaAvStartupIds& ids) noexcept;
    ::media::Status purge(const MediaAvGenerationPurge& purge);
    void reset() noexcept;

    const MediaAvSyncGroupKey& groupKey() const noexcept { return m_groupKey; }
    const std::optional<int>& audioSampleRate() const noexcept { return m_audioSampleRate; }

private:
    using Payloads = std::pmr::unordered_map<MediaAvStartupUnitId, MediaBufferRef, MediaAvStartupUnitIdHash>;
    using Seen = std::pmr::unordered_set<MediaAvStartupUnitId, MediaAvStartupUnitIdHash>;
    MediaLineageAllocationScope m_allocation;
    MediaAvStartupGenerationStoragePlan m_storage;
    MediaAvSyncGroupKey m_groupKey;
    MediaAvStartupHashMemoryResource m_payloadResource;
    MediaAvStartupHashMemoryResource m_seenResource;
    Payloads m_payloads;
    Seen m_seen;
    bool m_storagePrepared = false;
    std::optional<int> m_audioSampleRate;
    std::optional<std::uint64_t> m_generation;
    std::optional<std::uint64_t> m_lastTransitionSequence;
};

} // namespace media::ffmpeg::graph

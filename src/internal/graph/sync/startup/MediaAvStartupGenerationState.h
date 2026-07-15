#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvStartupGenerationState final : public MediaAvGenerationPurgeTarget {
public:
    explicit MediaAvStartupGenerationState(MediaAvSyncGroupKey groupKey);

    static constexpr std::string_view plannedIdentity() noexcept
    {
        return "startup_generation_state";
    }

    ::media::Status store(const MediaAvSyncGroupKey& groupKey,
                          const MediaAvStartupAccessUnit& unit,
                          MediaBufferRef media);
    ::media::Result<MediaBufferRef> take(const MediaAvStartupUnitId& id);
    void erase(const MediaAvStartupUnitId& id) noexcept;
    void erase(const std::vector<MediaAvStartupUnitId>& ids) noexcept;
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;
    void reset() noexcept;

    const MediaAvSyncGroupKey& groupKey() const noexcept { return m_groupKey; }
    const std::optional<int>& audioSampleRate() const noexcept { return m_audioSampleRate; }

private:
    MediaAvSyncGroupKey m_groupKey;
    std::unordered_map<MediaAvStartupUnitId, MediaBufferRef,
                       MediaAvStartupUnitIdHash> m_payloads;
    std::unordered_set<MediaAvStartupUnitId, MediaAvStartupUnitIdHash> m_seen;
    std::optional<int> m_audioSampleRate;
    std::optional<std::uint64_t> m_generation;
    std::optional<std::uint64_t> m_lastTransitionSequence;
};

} // namespace media::ffmpeg::graph

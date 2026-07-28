#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/lineage/MediaRetainedControlFreshness.h"

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaAudioLineageGenerationDisposition {
    Current,
    DropStale
};

class MediaAudioLineageState : public MediaAvGenerationPurgeTarget {
public:
    using Lock = std::unique_lock<std::recursive_mutex>;

    MediaAudioLineageState(bool synchronized, std::size_t capacity) noexcept;
    virtual ~MediaAudioLineageState() = default;

    ::media::Result<MediaAudioLineageGenerationDisposition>
    classifyObservation(std::uint64_t generation) const;
    ::media::Status validateObservation(std::uint64_t generation) const;
    ::media::Status observe(std::uint64_t generation);
    ::media::Status authorizeRetainedControl(const MediaBufferRef& buffer);
    bool retainedControlIsCurrent(const MediaBufferRef& buffer) const noexcept;
    bool pendingOutputIsCurrent(
        const MediaBufferRef& buffer,
        std::optional<std::uint64_t> mediaGeneration) const noexcept;
    void resetRetainedControlFreshness() noexcept;
    bool isCurrent(std::uint64_t generation) const noexcept;
    bool synchronized() const noexcept;
    std::size_t capacity() const noexcept;
    Lock lock() const;
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;

protected:
    void resetLifecycleLineage() noexcept;
    virtual void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept = 0;

private:
    mutable std::recursive_mutex m_mutex;
    bool m_synchronized = false;
    std::size_t m_capacity = 0;
    std::uint64_t m_generation = 0;
    std::uint64_t m_lastTransitionSequence = 0;
    MediaRetainedControlFreshness m_retainedControl;
};

} // namespace media::ffmpeg::graph

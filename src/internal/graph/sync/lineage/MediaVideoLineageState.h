#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"
#include "internal/graph/sync/lineage/MediaRetainedControlFreshness.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaVideoLineageState : public MediaAvGenerationPurgeTarget {
public:
    using Lock = std::unique_lock<std::recursive_mutex>;

    explicit MediaVideoLineageState(
        std::shared_ptr<MediaCodecLineageRegistry> registry) noexcept;
    MediaVideoLineageState(
        bool synchronized,
        std::shared_ptr<MediaCodecLineageRegistry> registry) noexcept;
    ~MediaVideoLineageState() override = default;

    [[nodiscard]] bool synchronized() const noexcept;
    [[nodiscard]] std::shared_ptr<MediaCodecLineageRegistry> registry() const noexcept;
    [[nodiscard]] Lock lock() const;
    ::media::Status validateObservation(std::uint64_t generation) const;
    ::media::Status observe(std::uint64_t generation);
    ::media::Status authorizeRetainedControl(const MediaBufferRef& buffer);
    [[nodiscard]] bool pendingOutputIsCurrent(
        const MediaBufferRef& buffer,
        std::optional<std::uint64_t> mediaGeneration) const noexcept;
    void resetGenerationLifecycle() noexcept;
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;

protected:
    virtual void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept = 0;

private:
    mutable std::recursive_mutex m_mutex;
    std::shared_ptr<MediaCodecLineageRegistry> m_registry;
    bool m_synchronized = false;
    MediaRetainedControlFreshness m_retainedControl;
    std::uint64_t m_generation = 0;
    std::uint64_t m_lastTransitionSequence = 0;
};

} // namespace media::ffmpeg::graph

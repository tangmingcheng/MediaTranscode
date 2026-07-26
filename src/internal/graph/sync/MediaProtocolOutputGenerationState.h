#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaProtocolOutputGenerationCommitReservation final {
public:
    MediaProtocolOutputGenerationCommitReservation(
        MediaProtocolOutputGenerationCommitReservation&&) noexcept = default;
    MediaProtocolOutputGenerationCommitReservation& operator=(
        MediaProtocolOutputGenerationCommitReservation&&) noexcept = default;
    MediaProtocolOutputGenerationCommitReservation(
        const MediaProtocolOutputGenerationCommitReservation&) = delete;
    MediaProtocolOutputGenerationCommitReservation& operator=(
        const MediaProtocolOutputGenerationCommitReservation&) = delete;

private:
    friend class MediaProtocolOutputGenerationState;

    explicit MediaProtocolOutputGenerationCommitReservation(
        std::unique_lock<std::mutex> lock) noexcept;

    std::unique_lock<std::mutex> m_lock;
};

class MediaProtocolOutputGenerationState final
    : public MediaAvGenerationPurgeTarget {
public:
    explicit MediaProtocolOutputGenerationState(std::string plannedIdentity);

    std::string_view plannedIdentity() const noexcept;
    ::media::Status permitActivatedGeneration(std::uint64_t generation);
    ::media::Result<MediaProtocolOutputGenerationCommitReservation>
    reserveCommit(std::uint64_t generation) const;
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;
    void resetLifecycle() noexcept;

private:
    mutable std::mutex m_mutex;
    std::string m_plannedIdentity;
    std::optional<std::uint64_t> m_permittedGeneration;
    std::optional<std::uint64_t> m_pendingGeneration;
    std::optional<std::uint64_t> m_pendingTransitionSequence;
    std::optional<std::uint64_t> m_lastTransitionSequence;
};

} // namespace media::ffmpeg::graph

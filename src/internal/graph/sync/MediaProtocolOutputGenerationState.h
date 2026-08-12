#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

class MediaProtocolOutputGenerationSessionState {
public:
    virtual ~MediaProtocolOutputGenerationSessionState() = default;

private:
    friend class MediaProtocolOutputGenerationState;
    virtual ::media::Status prepareForGenerationPurge()
    {
        return ::media::Status::success();
    }
    virtual void resetForGenerationPurge() noexcept = 0;
    mutable std::mutex m_mutex;
};

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
    bool startsAfterGenerationTransition() const noexcept;

private:
    friend class MediaProtocolOutputGenerationState;

    explicit MediaProtocolOutputGenerationCommitReservation(
        MediaProtocolOutputCommitReservation outputPermit,
        std::unique_lock<std::mutex> stateLock,
        std::unique_lock<std::mutex> sessionLock,
        std::optional<std::uint64_t> completedTransitionSequence) noexcept;

    MediaProtocolOutputCommitReservation m_outputPermit;
    std::unique_lock<std::mutex> m_stateLock;
    std::unique_lock<std::mutex> m_sessionLock;
    std::optional<std::uint64_t> m_completedTransitionSequence;
};

class MediaProtocolOutputGenerationSessionMutationReservation final {
public:
    MediaProtocolOutputGenerationSessionMutationReservation(
        MediaProtocolOutputGenerationSessionMutationReservation&&) noexcept =
            default;
    MediaProtocolOutputGenerationSessionMutationReservation& operator=(
        MediaProtocolOutputGenerationSessionMutationReservation&&) noexcept =
            default;
    MediaProtocolOutputGenerationSessionMutationReservation(
        const MediaProtocolOutputGenerationSessionMutationReservation&) =
            delete;
    MediaProtocolOutputGenerationSessionMutationReservation& operator=(
        const MediaProtocolOutputGenerationSessionMutationReservation&) =
            delete;

private:
    friend class MediaProtocolOutputGenerationState;
    MediaProtocolOutputGenerationSessionMutationReservation(
        std::unique_lock<std::mutex> stateLock,
        std::unique_lock<std::mutex> sessionLock) noexcept
        : m_stateLock(std::move(stateLock))
        , m_sessionLock(std::move(sessionLock))
    {
    }

    std::unique_lock<std::mutex> m_stateLock;
    std::unique_lock<std::mutex> m_sessionLock;
};

struct MediaProtocolOutputAuthorityActivation final {
    MediaProtocolOutputActivation activation;
    MediaProtocolOutputGenerationCommitReservation reservation;
};

class MediaProtocolOutputGenerationState final
    : public MediaAvGenerationPurgeTarget {
public:
    enum class GenerationDisposition {
        Old,
        Current,
        Future
    };

    MediaProtocolOutputGenerationState(
        std::string plannedIdentity,
        std::shared_ptr<MediaProtocolOutputGenerationSessionState>
            sessionState);

    std::string_view plannedIdentity() const noexcept;
    const std::shared_ptr<MediaProtocolOutputGenerationSessionState>&
    sessionState() const noexcept;
    ::media::Result<MediaProtocolOutputGenerationCommitReservation>
    permitActivatedGeneration(
        const MediaProtocolOutputRuntimeAuthority& authority,
        std::uint64_t generation,
        std::optional<std::uint64_t> transitionSequence);
    ::media::Result<MediaProtocolOutputAuthorityActivation>
    permitAuthorityActivation(
        const MediaProtocolOutputRuntimeAuthority& authority);
    ::media::Result<MediaProtocolOutputGenerationCommitReservation>
    reserveCommit(const MediaProtocolOutputRuntimeAuthority& authority,
                  std::uint64_t generation) const;
    ::media::Result<GenerationDisposition>
    classifyGeneration(std::uint64_t generation) const;
    MediaProtocolOutputGenerationSessionMutationReservation
    reserveSessionMutation() const;
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;
    ::media::Status resetLifecycle();

private:
    mutable std::mutex m_mutex;
    std::string m_plannedIdentity;
    std::optional<std::uint64_t> m_permittedGeneration;
    std::optional<std::uint64_t> m_pendingGeneration;
    std::optional<std::uint64_t> m_pendingTransitionSequence;
    std::optional<std::uint64_t> m_lastTransitionSequence;
    std::shared_ptr<MediaProtocolOutputGenerationSessionState> m_sessionState;
};

} // namespace media::ffmpeg::graph

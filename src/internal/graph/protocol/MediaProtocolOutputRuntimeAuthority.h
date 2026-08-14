#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"
#include "internal/graph/runtime/MediaNodeProcessResult.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "media_transcode/Result.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaProtocolOutputActivation final {
    MediaRunningTime sourceStart;
    MediaRunningTime masterRelease;
    std::uint64_t generation;
    std::optional<std::uint64_t> completedTransitionSequence;

    friend bool operator==(const MediaProtocolOutputActivation&,
                           const MediaProtocolOutputActivation&) = default;
};

class MediaProtocolOutputCommitReservation final {
public:
    explicit MediaProtocolOutputCommitReservation(
        MediaAvOutputPermitCommitReservation reservation) noexcept;
    explicit MediaProtocolOutputCommitReservation(
        std::unique_lock<std::mutex> reservation) noexcept;

    MediaProtocolOutputCommitReservation(
        MediaProtocolOutputCommitReservation&&) noexcept = default;
    MediaProtocolOutputCommitReservation& operator=(
        MediaProtocolOutputCommitReservation&&) noexcept = default;
    MediaProtocolOutputCommitReservation(
        const MediaProtocolOutputCommitReservation&) = delete;
    MediaProtocolOutputCommitReservation& operator=(
        const MediaProtocolOutputCommitReservation&) = delete;

private:
    std::variant<MediaAvOutputPermitCommitReservation,
                 std::unique_lock<std::mutex>> m_reservation;
};

class MediaProtocolOutputRuntimeAuthority : public MediaMasterClock {
public:
    virtual ~MediaProtocolOutputRuntimeAuthority() = default;
    virtual const MediaProtocolOutputSessionKey& sessionKey() const noexcept = 0;
    virtual MediaTranscodeStreamSet streamSet() const noexcept = 0;
    virtual ::media::Result<MediaProtocolOutputActivation>
    validateActivation(const MediaBufferRef& buffer) const = 0;
    virtual ::media::Result<MediaProtocolOutputActivation>
    currentActivation() const = 0;
    virtual ::media::Result<MediaProtocolOutputCommitReservation>
    reserveCommit(std::uint64_t generation) const = 0;
    virtual ::media::Result<MediaRunningTime> now() const noexcept = 0;
    virtual const std::shared_ptr<const MediaSharedNtpEpoch>&
    sharedNtpEpoch() const noexcept = 0;
    virtual MediaNodeProcessResult::DeadlineWait deadlineWait(
        MediaRunningTime deadline) const = 0;
    virtual void markAborted() noexcept = 0;
};

class MediaAvSyncGroupRuntime;

class MediaAvProtocolOutputRuntimeAuthority final
    : public MediaProtocolOutputRuntimeAuthority {
public:
    static ::media::Result<
        std::shared_ptr<MediaAvProtocolOutputRuntimeAuthority>> create(
        std::shared_ptr<MediaAvSyncGroupRuntime> group);

    const MediaProtocolOutputSessionKey& sessionKey() const noexcept override;
    MediaTranscodeStreamSet streamSet() const noexcept override;
    ::media::Result<MediaProtocolOutputActivation>
    validateActivation(const MediaBufferRef& buffer) const override;
    ::media::Result<MediaProtocolOutputActivation>
    currentActivation() const override;
    ::media::Result<MediaProtocolOutputCommitReservation>
    reserveCommit(std::uint64_t generation) const override;
    ::media::Result<MediaRunningTime> now() const noexcept override;
    const std::shared_ptr<const MediaSharedNtpEpoch>&
    sharedNtpEpoch() const noexcept override;
    MediaNodeProcessResult::DeadlineWait deadlineWait(
        MediaRunningTime deadline) const override;
    void markAborted() noexcept override;

private:
    MediaAvProtocolOutputRuntimeAuthority(
        MediaProtocolOutputSessionKey sessionKey,
        std::shared_ptr<MediaAvSyncGroupRuntime> group) noexcept;

    MediaProtocolOutputSessionKey m_sessionKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
};

class MediaVideoProtocolOutputRuntimeAuthority final
    : public MediaProtocolOutputRuntimeAuthority {
public:
    static ::media::Result<
        std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority>> create(
        MediaProtocolOutputSessionKey sessionKey,
        std::uint64_t initialGeneration);

    ::media::Result<MediaBufferRef> activate(
        MediaRunningTime sourceStart,
        MediaRunningTime transportLead);
    const MediaProtocolOutputSessionKey& sessionKey() const noexcept override;
    MediaTranscodeStreamSet streamSet() const noexcept override;
    ::media::Result<MediaProtocolOutputActivation>
    validateActivation(const MediaBufferRef& buffer) const override;
    ::media::Result<MediaProtocolOutputActivation>
    currentActivation() const override;
    ::media::Result<MediaProtocolOutputCommitReservation>
    reserveCommit(std::uint64_t generation) const override;
    ::media::Result<MediaRunningTime> now() const noexcept override;
    const std::shared_ptr<const MediaSharedNtpEpoch>&
    sharedNtpEpoch() const noexcept override;
    MediaNodeProcessResult::DeadlineWait deadlineWait(
        MediaRunningTime deadline) const override;
    void markAborted() noexcept override;

private:
    MediaVideoProtocolOutputRuntimeAuthority(
        MediaProtocolOutputSessionKey sessionKey,
        std::uint64_t initialGeneration,
        std::chrono::steady_clock::time_point steadyAnchor,
        std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch) noexcept;

    MediaProtocolOutputSessionKey m_sessionKey;
    std::uint64_t m_initialGeneration;
    std::chrono::steady_clock::time_point m_steadyAnchor;
    std::shared_ptr<const MediaSharedNtpEpoch> m_sharedNtpEpoch;
    mutable std::mutex m_mutex;
    std::optional<MediaProtocolOutputActivation> m_activation;
    bool m_aborted = false;
};

} // namespace media::ffmpeg::graph

#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoOutputActivatedBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <chrono>
#include <utility>

namespace media::ffmpeg::graph {

MediaProtocolOutputCommitReservation::MediaProtocolOutputCommitReservation(
    MediaAvOutputPermitCommitReservation reservation) noexcept
    : m_reservation(std::in_place_type<MediaAvOutputPermitCommitReservation>,
                    std::move(reservation))
{
}

MediaProtocolOutputCommitReservation::MediaProtocolOutputCommitReservation(
    std::unique_lock<std::mutex> reservation) noexcept
    : m_reservation(std::in_place_type<std::unique_lock<std::mutex>>,
                    std::move(reservation))
{
}

MediaAvProtocolOutputRuntimeAuthority::
MediaAvProtocolOutputRuntimeAuthority(
    MediaProtocolOutputSessionKey sessionKey,
    std::shared_ptr<MediaAvSyncGroupRuntime> group) noexcept
    : m_sessionKey(std::move(sessionKey)), m_group(std::move(group))
{
}

::media::Result<std::shared_ptr<MediaAvProtocolOutputRuntimeAuthority>>
MediaAvProtocolOutputRuntimeAuthority::create(
    std::shared_ptr<MediaAvSyncGroupRuntime> group)
{
    if (!group || !group->key().valid() || !group->clock()) {
        return ::media::Result<
            std::shared_ptr<MediaAvProtocolOutputRuntimeAuthority>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V protocol output authority requires one registered sync group"));
    }
    MediaProtocolOutputSessionKey sessionKey(group->key().value());
    return ::media::Result<
        std::shared_ptr<MediaAvProtocolOutputRuntimeAuthority>>::success(
        std::shared_ptr<MediaAvProtocolOutputRuntimeAuthority>(
            new MediaAvProtocolOutputRuntimeAuthority(
                std::move(sessionKey), std::move(group))));
}

const MediaProtocolOutputSessionKey&
MediaAvProtocolOutputRuntimeAuthority::sessionKey() const noexcept
{
    return m_sessionKey;
}

MediaTranscodeStreamSet
MediaAvProtocolOutputRuntimeAuthority::streamSet() const noexcept
{
    return MediaTranscodeStreamSet::AudioVideo;
}

::media::Result<MediaProtocolOutputActivation>
MediaAvProtocolOutputRuntimeAuthority::validateActivation(
    const MediaBufferRef& buffer) const
{
    const auto* activated =
        dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(buffer.get());
    if (!activated || activated->groupKey() != m_group->key() ||
        m_group->lifecycleState() !=
            MediaAvSyncGroupRuntime::LifecycleState::Active) {
        return ::media::Result<MediaProtocolOutputActivation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V protocol output authority rejects mismatched activation"));
    }
    auto current = m_group->playbackEpoch();
    if (!current || current.value() != activated->epoch() ||
        !m_group->sharedNtpEpoch()) {
        return ::media::Result<MediaProtocolOutputActivation>::failure(
            current ? ::media::ErrorInfo::invalidArgument(
                          "A/V protocol output activation differs from its group")
                    : current.error());
    }
    return ::media::Result<MediaProtocolOutputActivation>::success(
        MediaProtocolOutputActivation{
            current.value().sourceStart,
            current.value().masterRelease,
            current.value().generation,
            activated->completedTransitionSequence()});
}

::media::Result<MediaProtocolOutputActivation>
MediaAvProtocolOutputRuntimeAuthority::currentActivation() const
{
    auto epoch = m_group->playbackEpoch();
    if (!epoch || !m_group->sharedNtpEpoch()) {
        return ::media::Result<MediaProtocolOutputActivation>::failure(
            epoch ? ::media::ErrorInfo::notInitialized(
                        "A/V protocol output authority has no shared NTP epoch")
                  : epoch.error());
    }
    return ::media::Result<MediaProtocolOutputActivation>::success(
        MediaProtocolOutputActivation{
            epoch.value().sourceStart,
            epoch.value().masterRelease,
            epoch.value().generation,
            m_group->epochTransitionSnapshot()
                .completedTransitionSequence});
}

::media::Result<MediaProtocolOutputCommitReservation>
MediaAvProtocolOutputRuntimeAuthority::reserveCommit(
    std::uint64_t generation) const
{
    auto permit = m_group->reserveOutputCommit(generation);
    if (!permit) {
        return ::media::Result<MediaProtocolOutputCommitReservation>::failure(
            permit.error());
    }
    return ::media::Result<MediaProtocolOutputCommitReservation>::success(
        MediaProtocolOutputCommitReservation(std::move(permit).value()));
}

::media::Result<MediaRunningTime>
MediaAvProtocolOutputRuntimeAuthority::now() const noexcept
{
    return m_group->clock()->now();
}

const std::shared_ptr<const MediaSharedNtpEpoch>&
MediaAvProtocolOutputRuntimeAuthority::sharedNtpEpoch() const noexcept
{
    return m_group->sharedNtpEpoch();
}

MediaNodeProcessResult::DeadlineWait
MediaAvProtocolOutputRuntimeAuthority::deadlineWait(
    MediaRunningTime deadline) const
{
    return MediaNodeProcessResult::DeadlineWait(m_group->key(), deadline);
}

void MediaAvProtocolOutputRuntimeAuthority::markAborted() noexcept
{
    m_group->markAborted();
}

MediaVideoProtocolOutputRuntimeAuthority::
MediaVideoProtocolOutputRuntimeAuthority(
    MediaProtocolOutputSessionKey sessionKey,
    std::chrono::steady_clock::time_point steadyAnchor,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_steadyAnchor(steadyAnchor),
      m_sharedNtpEpoch(std::move(sharedNtpEpoch))
{
}

::media::Result<std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority>>
MediaVideoProtocolOutputRuntimeAuthority::create(
    MediaProtocolOutputSessionKey sessionKey)
{
    if (!sessionKey.valid()) {
        return ::media::Result<std::shared_ptr<
            MediaVideoProtocolOutputRuntimeAuthority>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly protocol output authority requires a session key"));
    }
    const auto steadyAnchor = std::chrono::steady_clock::now();
    const auto wallAnchor =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    auto ntp = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), wallAnchor);
    if (!ntp) {
        return ::media::Result<std::shared_ptr<
            MediaVideoProtocolOutputRuntimeAuthority>>::failure(ntp.error());
    }
    return ::media::Result<std::shared_ptr<
        MediaVideoProtocolOutputRuntimeAuthority>>::success(
        std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority>(
            new MediaVideoProtocolOutputRuntimeAuthority(
                std::move(sessionKey), steadyAnchor,
                std::make_shared<const MediaSharedNtpEpoch>(
                    std::move(ntp).value()))));
}

::media::Result<MediaBufferRef>
MediaVideoProtocolOutputRuntimeAuthority::activate(
    MediaRunningTime sourceStart,
    MediaRunningTime transportLead)
{
    auto current = now();
    if (!current) return ::media::Result<MediaBufferRef>::failure(current.error());
    auto release = current.value().checkedAdd(transportLead);
    if (!release) {
        return ::media::Result<MediaBufferRef>::failure(release.error());
    }
    MediaProtocolOutputActivation activation{
        sourceStart, release.value(), 1, std::nullopt};
    auto buffer = MediaVideoOutputActivatedBuffer::create(
        m_sessionKey, activation);
    if (!buffer) return buffer;
    std::lock_guard lock(m_mutex);
    if (m_aborted || m_activation) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly protocol output authority rejects duplicate activation"));
    }
    m_activation = activation;
    return buffer;
}

const MediaProtocolOutputSessionKey&
MediaVideoProtocolOutputRuntimeAuthority::sessionKey() const noexcept
{
    return m_sessionKey;
}

MediaTranscodeStreamSet
MediaVideoProtocolOutputRuntimeAuthority::streamSet() const noexcept
{
    return MediaTranscodeStreamSet::VideoOnly;
}

::media::Result<MediaProtocolOutputActivation>
MediaVideoProtocolOutputRuntimeAuthority::validateActivation(
    const MediaBufferRef& buffer) const
{
    const auto* activated =
        dynamic_cast<const MediaVideoOutputActivatedBuffer*>(buffer.get());
    std::lock_guard lock(m_mutex);
    if (!activated || activated->sessionKey() != m_sessionKey ||
        m_aborted || !m_activation ||
        activated->activation() != *m_activation) {
        return ::media::Result<MediaProtocolOutputActivation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly protocol output authority rejects mismatched activation"));
    }
    return ::media::Result<MediaProtocolOutputActivation>::success(
        *m_activation);
}

::media::Result<MediaProtocolOutputActivation>
MediaVideoProtocolOutputRuntimeAuthority::currentActivation() const
{
    std::lock_guard lock(m_mutex);
    if (m_aborted || !m_activation) {
        return ::media::Result<MediaProtocolOutputActivation>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly protocol output authority is not active"));
    }
    return ::media::Result<MediaProtocolOutputActivation>::success(
        *m_activation);
}

::media::Result<MediaProtocolOutputCommitReservation>
MediaVideoProtocolOutputRuntimeAuthority::reserveCommit(
    std::uint64_t generation) const
{
    std::unique_lock lock(m_mutex);
    if (m_aborted || !m_activation || generation != m_activation->generation) {
        return ::media::Result<MediaProtocolOutputCommitReservation>::failure(
            ::media::ErrorInfo::cancelled(
                "VideoOnly protocol output commit requires its active fixed generation"));
    }
    return ::media::Result<MediaProtocolOutputCommitReservation>::success(
        MediaProtocolOutputCommitReservation(std::move(lock)));
}

::media::Result<MediaRunningTime>
MediaVideoProtocolOutputRuntimeAuthority::now() const noexcept
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - m_steadyAnchor);
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(elapsed.count()));
}

const std::shared_ptr<const MediaSharedNtpEpoch>&
MediaVideoProtocolOutputRuntimeAuthority::sharedNtpEpoch() const noexcept
{
    return m_sharedNtpEpoch;
}

MediaNodeProcessResult::DeadlineWait
MediaVideoProtocolOutputRuntimeAuthority::deadlineWait(
    MediaRunningTime deadline) const
{
    return MediaNodeProcessResult::DeadlineWait(
        m_steadyAnchor +
        std::chrono::nanoseconds(deadline.nanoseconds()));
}

void MediaVideoProtocolOutputRuntimeAuthority::markAborted() noexcept
{
    std::lock_guard lock(m_mutex);
    m_aborted = true;
}

} // namespace media::ffmpeg::graph

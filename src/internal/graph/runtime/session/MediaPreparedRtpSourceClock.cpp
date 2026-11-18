#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {
::media::Status MediaPreparedRtpSourceSession::advanceRtpCaptureBoundary(
    Stream& stream, std::int64_t observedAtNs)
{
    if (observedAtNs <= 0 || (stream.rtpCaptureExclusiveBeforeNs &&
        observedAtNs < *stream.rtpCaptureExclusiveBeforeNs))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP capture boundary must preserve its owner's observation order"));
    stream.rtpCaptureExclusiveBeforeNs = observedAtNs;
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::observeRtpCaptureIdle(
    MediaStreamKind kind, std::int64_t observedAtNs)
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_bootstrap) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "RTP capture idle boundary precedes live owner handoff"));
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    return advanceRtpCaptureBoundary(*selected.value(), observedAtNs);
}

::media::Status MediaPreparedRtpSourceSession::bindRuntimeGroup(
    const std::shared_ptr<MediaAvSyncGroupRuntime>& group)
{
    // Only immutable group facts are inspected while holding the source lock.
    // Runtime arbitration must always be acquired outside this mutex.
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (!m_frozen || m_runtimeGroup || m_runtimeClockGeneration || !group ||
        group->clock() != m_clock || !group->plan().members ||
        *group->plan().members != m_plan.video.binder.members)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP runtime group requires a single sealed binding with matching clock and members"));
    m_runtimeGroup.emplace(group);
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::observeRuntimeClockGroup(
    const MediaRtpClockGroupSnapshot& snapshot)
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (!m_frozen || m_group)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Runtime clock observation requires completed prepared clock handoff"));
    if (auto status = snapshot.validateMembers(m_plan.video.binder.members); !status) return status;
    if (snapshot.state != MediaRtpClockGroupState::Locked) {
        m_runtimeClockLocked = false;
        return ::media::Status::success();
    }
    if (m_runtimeClockGeneration && snapshot.groupGeneration < *m_runtimeClockGeneration)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared runtime clock generation regressed"));
    const auto check = [&](const Stream& stream, const MediaRtpSourceClockCalibration& member) {
        return !m_runtimeClockGeneration ||
            snapshot.groupGeneration != *m_runtimeClockGeneration ||
            (stream.runtimeIngressGeneration == member.generation && stream.runtimeSsrc == member.ssrc);
    };
    if (!check(*m_video, snapshot.locked->video) ||
        (m_audio && !check(*m_audio, *snapshot.locked->audio)))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared runtime clock changed a member within its frozen generation"));
    m_video->runtimeIngressGeneration = snapshot.locked->video.generation;
    m_video->runtimeSsrc = snapshot.locked->video.ssrc;
    if (m_audio) {
        m_audio->runtimeIngressGeneration = snapshot.locked->audio->generation;
        m_audio->runtimeSsrc = snapshot.locked->audio->ssrc;
    }
    m_runtimeClockGeneration = snapshot.groupGeneration;
    m_runtimeClockLocked = true;
    // These are observation facts only. Source activation/publication authority
    // must be acquired outside this mutex before a closing transaction begins.
    return ::media::Status::success();
}
::media::Status MediaPreparedRtpSourceSession::observeConsumedControl(
    MediaStreamKind kind, std::uint64_t sequence, const std::optional<MediaRtpClockInvalidation>& invalidation)
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    auto& s = *selected.value();
    if (!m_frozen || m_group || sequence == 0 || sequence >= s.nextSequence ||
        !s.lastControlSequence || sequence > *s.lastControlSequence ||
        (s.consumedControlSequence && sequence <= *s.consumedControlSequence) ||
        (invalidation && (invalidation->generation == 0 || invalidation->observedAtNs <= 0)))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP control receipt requires its successfully consumed ordered event"));
    s.consumedControlSequence = sequence;
    if (invalidation && (invalidation->reason == MediaRtpSourceUnavailableReason::SenderLeft ||
        invalidation->reason == MediaRtpSourceUnavailableReason::GroupClosed)) {
        s.consumedRetirementSequence = sequence;
        s.consumedRetirement = *invalidation;
    }
    finishRetirementsLocked();
    // A real control consumption can release a closing barrier on either member.
    for (auto* member : {m_video.get(), m_audio.get()}) {
        if (!member) continue;
        notifyCredit(*member);
        if (member->wakeup) member->wakeup->notify();
    }
    m_ready.notify_all();
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph

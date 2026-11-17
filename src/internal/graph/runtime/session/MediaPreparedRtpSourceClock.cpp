#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

namespace media::ffmpeg::graph {
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
::media::Status MediaPreparedRtpSourceSession::observeConsumedRetirement(
    MediaStreamKind kind, std::uint64_t sequence, const MediaRtpClockInvalidation& invalidation)
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    auto& s = *selected.value();
    if (!m_frozen || m_group || sequence == 0 || sequence >= s.nextSequence ||
        (s.consumedRetirementSequence && sequence <= *s.consumedRetirementSequence) ||
        invalidation.generation == 0 || invalidation.observedAtNs <= 0 ||
        (invalidation.reason != MediaRtpSourceUnavailableReason::SenderLeft &&
            invalidation.reason != MediaRtpSourceUnavailableReason::GroupClosed))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP retirement receipt requires an ordered consumed member invalidation"));
    s.consumedRetirementSequence = sequence;
    s.consumedRetirement = invalidation;
    // Both source members may be waiting for the last retirement consumption.
    for (auto* member : {m_video.get(), m_audio.get()}) {
        if (!member) continue;
        notifyCredit(*member);
        if (member->wakeup) member->wakeup->notify();
    }
    m_ready.notify_all();
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph

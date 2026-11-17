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
} // namespace media::ffmpeg::graph

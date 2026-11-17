#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalidDrain(const char* text)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text)); }
bool validMember(const MediaAvSourceDrainMember& member)
{
    // Ingress trackers start at zero; playback/clock-group nonzero rules do not
    // apply to this independently validated protocol identity.
    return member.observedAtNs >= 0 &&
        member.closeDeadlineNs > member.observedAtNs &&
        (member.reason == MediaAvSourceCloseReason::SenderLeft ||
         member.reason == MediaAvSourceCloseReason::GroupClose);
}
bool sameMember(const MediaAvSourceDrainMember& left, const MediaAvSourceDrainMember& right)
{ return left.ingressGeneration == right.ingressGeneration && left.ssrc == right.ssrc; }
}

::media::Status MediaAvEpochTransitionService::beginSourceDrain(MediaAvSourceDrainRequest request)
{
    std::lock_guard lock(m_mutex);
    if (m_firstError) return ::media::Status::failure(*m_firstError);
    if (m_aborted || !m_coordinator || m_readiness != MediaAvGenerationReadiness::Locked ||
        !m_activation || request.playbackGeneration != m_activation->epoch().generation ||
        request.playbackGeneration == std::numeric_limits<std::uint64_t>::max() ||
        !outputPermittedLocked(request.playbackGeneration))
        return invalidDrain("Source drain requires the exact live activated source generation");
    if (request.audio.has_value() != (m_members == MediaTranscodeStreamSet::AudioVideo) ||
        !validMember(request.video) || (request.audio && !validMember(*request.audio)) ||
        (request.video.reason != MediaAvSourceCloseReason::SenderLeft &&
         (!request.audio || request.audio->reason != MediaAvSourceCloseReason::SenderLeft)))
        return invalidDrain("Source drain requires complete member mapping and a sender departure");
    if (m_sourceDrain) {
        if (m_sourceDrain->request.playbackGeneration != request.playbackGeneration ||
            !sameMember(m_sourceDrain->request.video, request.video) ||
            (request.audio && !sameMember(*m_sourceDrain->request.audio, *request.audio)))
            return invalidDrain("Source drain cannot replace an active closing generation or member");
        return ::media::Status::success(); // Repeated BYE never renews a deadline.
    }
    m_sourceDrain.emplace(MediaAvSourceDrainSnapshot{request, {},
        request.audio ? std::optional(MediaAvSourceDrainMemberProgress{}) : std::nullopt});
    return ::media::Status::success();
}

::media::Status MediaAvEpochTransitionService::sealSourceDrainInput(
    std::uint64_t generation, MediaStreamKind stream, std::uint64_t ingressGeneration,
    std::uint64_t acceptedFrontier)
{
    std::lock_guard lock(m_mutex);
    if (m_firstError) return ::media::Status::failure(*m_firstError);
    if (m_aborted || !m_sourceDrain || m_sourceDrain->request.playbackGeneration != generation ||
        !outputPermittedLocked(generation)) return invalidDrain("Source input seal lacks its live closing generation");
    MediaAvSourceDrainMemberProgress* progress = nullptr;
    const MediaAvSourceDrainMember* member = nullptr;
    if (stream == MediaStreamKind::Video) { progress = &m_sourceDrain->video; member = &m_sourceDrain->request.video; }
    else if (stream == MediaStreamKind::Audio && m_sourceDrain->audio) {
        progress = &*m_sourceDrain->audio; member = &*m_sourceDrain->request.audio;
    }
    if (!member || member->ingressGeneration != ingressGeneration)
        return invalidDrain("Source input seal has an unmatched member ingress generation");
    if (progress->acceptedFrontier && *progress->acceptedFrontier != acceptedFrontier)
        return invalidDrain("Source input seal cannot change its accepted frontier");
    progress->acceptedFrontier = acceptedFrontier;
    return ::media::Status::success();
}

::media::Result<bool> MediaAvEpochTransitionService::acknowledgeSourceDrained(
    std::uint64_t generation, MediaStreamKind stream)
{
    using Result = ::media::Result<bool>;
    std::lock_guard lock(m_mutex);
    if (m_firstError) return Result::failure(*m_firstError);
    if (m_aborted || !m_sourceDrain || m_sourceDrain->request.playbackGeneration != generation ||
        !outputPermittedLocked(generation)) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source drain acknowledgement lacks its live closing generation"));
    auto* progress = stream == MediaStreamKind::Video ? &m_sourceDrain->video :
        stream == MediaStreamKind::Audio && m_sourceDrain->audio ? &*m_sourceDrain->audio : nullptr;
    if (!progress || !progress->acceptedFrontier) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Source drain acknowledgement must follow the exact member input seal"));
    progress->drained = true;
    return Result::success(m_sourceDrain->complete());
}

std::optional<MediaAvSourceDrainSnapshot> MediaAvEpochTransitionService::sourceDrainSnapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    return m_sourceDrain;
}
} // namespace media::ffmpeg::graph

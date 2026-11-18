#include "internal/graph/nodes/input/RawRtpInputNode.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {
::media::Result<MediaOutputCommitReservation> RawRtpInputNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    auto generation = mediaEndGeneration(buffer);
    if (!generation) return Result::failure(generation.error());
    if (!generation.value()) return FFmpegNodeRuntime::reserveOutputCommit(buffer);
    if (!m_preparedSource || !m_syncGroup || !m_ingressBinding || !m_generationEnded)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Raw RTP generation end requires its prepared source lifecycle owner"));
    auto authority = m_syncGroup->reserveGenerationArbitration();
    if (!authority) return Result::failure(authority.error());
    auto disposition = classifyMediaAvGenerationEvidence(authority.value().reacquisition(),
        authority.value().epoch(), *generation.value());
    if (!disposition) return Result::failure(disposition.error());
    if (disposition.value() == MediaAvGenerationEvidenceDisposition::Retired)
        return Result::failure(::media::ErrorInfo::cancelled("Raw RTP retires obsolete generation end"));
    const auto closing = m_syncGroup->sourceDrainSnapshot();
    if (disposition.value() != MediaAvGenerationEvidenceDisposition::Target ||
        !authority.value().epoch().outputPermitted || !closing ||
        closing->request.playbackGeneration != *generation.value())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Raw RTP generation end requires its published closing generation"));
    const auto* member = m_config.streamKind == MediaStreamKind::Video ? &closing->video :
        m_config.streamKind == MediaStreamKind::Audio && closing->audio ? &*closing->audio : nullptr;
    if (!member || !member->acceptedFrontier)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Raw RTP generation end precedes its member input frontier"));
    return Result::success(MediaOutputCommitReservation::hold(
        std::move(authority).value().retainPublicationAuthority()));
}
} // namespace media::ffmpeg::graph

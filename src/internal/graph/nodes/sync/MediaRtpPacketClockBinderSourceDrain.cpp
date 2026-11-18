#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {
::media::Status MediaRtpPacketClockBinderNode::endGeneration(
    MediaGraphExecutionContext& context, MediaBufferRef terminal)
{
    const auto invalid = [](const char* text) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text));
    };
    auto generation = mediaEndGeneration(terminal);
    if (!generation) return ::media::Status::failure(generation.error());
    if (!generation.value() || m_generationEnd)
        return invalid("RTP binder requires one generation end per source closing");
    {
        // Raw publishes this control in the frozen clock-group domain, after
        // validating its member ingress identity and sealing its media frontier.
        // Raw packet provenance remains in the separate ingress domain.
        auto authority = m_syncGroup->reserveGenerationArbitration();
        if (!authority) return ::media::Status::failure(authority.error());
        auto disposition = classifyMediaAvGenerationEvidence(authority.value().reacquisition(),
            authority.value().epoch(), *generation.value());
        if (!disposition) return ::media::Status::failure(disposition.error());
        if (disposition.value() == MediaAvGenerationEvidenceDisposition::Retired)
            return ::media::Status::success();
        const auto& epoch = authority.value().epoch();
        if (disposition.value() != MediaAvGenerationEvidenceDisposition::Target ||
            !epoch.outputPermitted || !epoch.playbackEpoch() ||
            epoch.playbackEpoch()->generation != *generation.value())
            return invalid("RTP binder generation end has no published source epoch");
        const auto closing = m_syncGroup->sourceDrainSnapshot();
        const auto& snapshot = m_clockSession->m_lockedSnapshot;
        if (!closing || closing->request.playbackGeneration != *generation.value() ||
            !snapshot || !snapshot->locked || snapshot->groupGeneration != *generation.value())
            return invalid("RTP binder generation end lacks its frozen clock-to-playback mapping");
        const auto* member = m_streamKind == MediaStreamKind::Video ? &closing->request.video :
            closing->request.audio ? &*closing->request.audio : nullptr;
        const auto* progress = m_streamKind == MediaStreamKind::Video ? &closing->video :
            closing->audio ? &*closing->audio : nullptr;
        const auto* calibration = m_streamKind == MediaStreamKind::Video ? &snapshot->locked->video :
            snapshot->locked->audio ? &*snapshot->locked->audio : nullptr;
        if (!member || !progress || !progress->acceptedFrontier || !calibration ||
            calibration->generation != member->ingressGeneration || calibration->ssrc != member->ssrc)
            return invalid("RTP binder generation end differs from its sealed protocol member");
        if (!m_clockSession->m_acquiringPackets->empty())
            return invalid("RTP binder generation end precedes clock-bound acquired packets");
        m_generationEnd = terminal;
    }
    // Publication reacquires authority; never retain arbitration over emission.
    // The existing timing product alone decides whether a lookahead tail exists.
    return finishPacketInput(context, std::move(terminal));
}
} // namespace media::ffmpeg::graph

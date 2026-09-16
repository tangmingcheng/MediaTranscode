#include "internal/graph/protocol/MediaProtocolPurgeInput.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

namespace media::ffmpeg::graph {

::media::Status authorizeProtocolPurgeInput(
    const MediaBufferRef& buffer, const MediaProtocolOutputSessionKey& session,
    const MediaAvGenerationPurge& purge,
    const std::optional<MediaAvGenerationPurge>& previousPurge)
{
    std::optional<std::uint64_t> generation;
    bool sessionMatches = true;
    const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(buffer.get());
    if (transport) {
        generation = transport->plan().shaping.generation();
        sessionMatches = transport->plan().shaping.sessionKey() == session.value();
    } else if (const auto* protocol = dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(buffer.get())) {
        generation = protocol->activation().generation;
        sessionMatches = protocol->sessionKey() == session;
    } else if (const auto* activation = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(buffer.get())) {
        generation = activation->epoch().generation;
        sessionMatches = activation->groupKey().value() == session.value();
    } else if (const auto* batch = dynamic_cast<const MediaMpegTsProtocolDatagramBatchBuffer*>(buffer.get())) {
        generation = batch->generation();
    } else if (const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(buffer.get())) {
        generation = scheduled->generation();
    } else if (const auto* control = dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
        if (control->controlKind() == MediaControlBufferKind::Abort)
            return ::media::Status::failure(::media::ErrorInfo::cancelled("Protocol input aborted during purge"));
        generation = control->generation();
    }
    const MediaAvGenerationPurge* authorized = generation && *generation == purge.oldGeneration
        ? &purge : generation && previousPurge && *generation == previousPurge->oldGeneration
            ? &*previousPurge : nullptr;
    if (!authorized || !sessionMatches)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Protocol queued input differs from exact purge authorization"));
    if (transport) return transport->globalSequence()->authorizeGenerationPurge(*authorized);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

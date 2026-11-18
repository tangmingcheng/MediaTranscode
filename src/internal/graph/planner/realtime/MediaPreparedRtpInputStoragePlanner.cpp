#include "internal/graph/planner/realtime/MediaPreparedRtpInputStoragePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
namespace media::ffmpeg::graph {
::media::Result<MediaPreparedRtpInputStorageFacts> MediaPreparedRtpInputStoragePlanner::plan(
    const MediaRealtimeRawInputPlan& raw, const MediaRtpIngressPlan* video,
    const MediaRtpIngressPlan* audio, const MediaPreparedRealtimeInput* preparedVideo,
    const MediaPreparedRealtimeInput* preparedAudio)
{
    using Result = ::media::Result<MediaPreparedRtpInputStorageFacts>;
    MediaPreparedRtpInputStorageFacts facts{{
        MediaPreparedInputPayloadSource::RawRtpAccessUnit,
        raw.videoAccessUnitEnvelope.maximumAccessUnitsPerPush,
        raw.videoAccessUnitEnvelope.completionAuthority,
        {{MediaStreamKind::Video, raw.videoAccessUnitEnvelope.maximumAccessUnitBytes,
          raw.videoAccessUnitEnvelope.sizeAuthority}}}, 0, {}};
    if (raw.audioAccessUnitEnvelope) {
        facts.payload.maximumPayloadsPerInputCompletion = (std::max)(
            facts.payload.maximumPayloadsPerInputCompletion,
            raw.audioAccessUnitEnvelope->maximumAccessUnitsPerPush);
        facts.payload.completionAuthority += "+" + raw.audioAccessUnitEnvelope->completionAuthority;
        facts.payload.streams.push_back({MediaStreamKind::Audio,
            raw.audioAccessUnitEnvelope->maximumAccessUnitBytes,
            raw.audioAccessUnitEnvelope->sizeAuthority});
    }
    if ((video != nullptr) != (preparedVideo != nullptr) ||
        (audio != nullptr) != (preparedAudio != nullptr) ||
        (preparedAudio && !preparedVideo))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP storage requires matching ingress and owned inputs"));
    if (preparedVideo) {
        auto reservation = preparedVideo->rawRtpPreparationStorage();
        if (!reservation) return Result::failure(reservation.error());
        if (!reservation.value().allocationOwner || !reservation.value().maximumAllocationBytes ||
            reservation.value().authority.empty() || reservation.value().streamCount != (preparedAudio ? 2u : 1u))
            return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP storage member count differs"));
        if (preparedAudio) {
            auto companion = preparedAudio->rawRtpPreparationStorage();
            if (!companion) return Result::failure(companion.error());
            if (companion.value().allocationOwner != reservation.value().allocationOwner ||
                companion.value().maximumAllocationBytes != reservation.value().maximumAllocationBytes ||
                companion.value().streamCount != reservation.value().streamCount ||
                companion.value().authority != reservation.value().authority)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Prepared RTP AV inputs do not share one allocation budget"));
        }
        // Both receive arenas, adapter state/control blocks and prepared input
        // owners now allocate from this same admitted preparation resource.
        // Its capacity already covers their coexistence; adding batch bytes
        // again would charge the same owners twice.
        facts.reservedBytes = reservation.value().maximumAllocationBytes;
        facts.authority = reservation.value().authority;
        facts.preparation = std::move(reservation).value();
    }
    return Result::success(std::move(facts));
}
} // namespace media::ffmpeg::graph

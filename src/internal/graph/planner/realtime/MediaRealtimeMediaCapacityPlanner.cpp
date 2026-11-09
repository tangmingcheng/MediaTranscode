#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedInputRetentionPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeMediaCapacityPlan>
MediaRealtimeMediaCapacityPlanner::plan(
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (ledger.entries.empty() ||
        ledger.media.videoUnits == 0 || ledger.media.videoUnitBytes == 0 ||
        ledger.media.videoBytes == 0) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime media capacity requires an admitted graph resource ledger"));
    }
    return ::media::Result<MediaRealtimeMediaCapacityPlan>::success(
        ledger.media);
}

::media::Result<MediaRealtimeMediaCapacityPlan> MediaRealtimeMediaCapacityPlanner::planDecodedSource(
    const MediaHardwareDescriptor& decoderOutput, std::size_t videoUnits,
    const MediaPreparedInputRetentionStream* audio, const MediaRealtimeDeploymentLatencyBudget& latency)
{
    using Result = ::media::Result<MediaRealtimeMediaCapacityPlan>;
    // A positive decoded presentation span needs at least two display points.
    // This lower bound does not promise the unchanged startup preroll duration.
    if (videoUnits < 2 || videoUnits > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        (decoderOutput.frameKind != MediaHardwareFrameKind::Software && !decoderOutput.isHardwareBacked()) ||
        latency.maximumResidence.nanoseconds() <= 0 || latency.authority.empty() ||
        (audio && (!audio->valid() || audio->maximumUnits > static_cast<std::size_t>((std::numeric_limits<int>::max)()))))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoded startup requires prepared frame, retention and deployment facts"));
    auto unit = MediaVideoSurfaceFootprint::logicalBytes(decoderOutput.size.width, decoderOutput.size.height,
        decoderOutput.isHardwareBacked() ? decoderOutput.surfacePixelFormat : decoderOutput.pixelFormat);
    if (!unit) return Result::failure(unit.error());
    auto bytes = MediaCheckedArithmetic::multiply(videoUnits, unit.value(), "decoded startup frame payload");
    if (!bytes) return Result::failure(bytes.error());
    const auto maximum = static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
    if (bytes.value() > maximum || (audio && (audio->maximumUnitBytes > maximum || audio->maximumBytes > maximum)))
        return Result::failure(::media::ErrorInfo::invalidArgument("Decoded startup exceeds the runtime byte range"));
    return Result::success({videoUnits, unit.value(), bytes.value(),
        audio ? std::optional{audio->maximumUnits} : std::nullopt,
        audio ? std::optional{audio->maximumUnitBytes} : std::nullopt,
        audio ? std::optional{audio->maximumBytes} : std::nullopt,
        latency.maximumResidence});
}

} // namespace media::ffmpeg::graph

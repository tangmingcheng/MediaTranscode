#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"

#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"
#include "internal/graph/planner/realtime/MediaRtpIngressCapabilityMaterializer.h"

#include <chrono>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpIngressPlan> MediaPreparedRtpIngressPlanner::plan(
    MediaPreparedRealtimeInput& input)
{
    using Result = ::media::Result<MediaRtpIngressPlan>;
    auto observation = input.rawRtpIngressObservation();
    if (!observation) return Result::failure(observation.error());
    auto socketCapacity = input.rawRtpEffectiveSocketReceivePayloadBytes();
    if (!socketCapacity) return Result::failure(socketCapacity.error());
    auto capability = MediaRtpIngressCapabilityMaterializer::materialize(
        socketCapacity.value());
    if (!capability) return Result::failure(capability.error());
    auto byteCapacity = input.rawRtpPreparedByteCapacity();
    if (!byteCapacity) return Result::failure(byteCapacity.error());
    auto datagramBytes = input.rawRtpMaximumDatagramBytes();
    if (!datagramBytes) return Result::failure(datagramBytes.error());
    return MediaRtpIngressPlan::create(
        capability.value(), observation.value(), byteCapacity.value(),
        datagramBytes.value());
}

::media::Status MediaPreparedRtpIngressPlanner::bind(
    const MediaRtpIngressPlan& ingress,
    MediaRealtimeRtpTransportPlan& transport)
{
    if (auto status = ingress.validateProduct(); !status) return status;
    const auto delay = std::chrono::ceil<std::chrono::milliseconds>(
        std::chrono::nanoseconds(ingress.maximumReorderDelayNanoseconds()));
    constexpr auto maximum = (std::numeric_limits<int>::max)();
    if (ingress.socketReceiveCapacityBytes() > static_cast<std::size_t>(maximum) ||
        ingress.maximumDatagramBytes() > static_cast<std::size_t>(maximum) ||
        ingress.reorderWindowPackets() > static_cast<std::size_t>(maximum) ||
        delay.count() > maximum) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "planner-bound RTP ingress facts exceed node option range"));
    }
    transport.ingress = ingress;
    transport.receiveBufferBytes = static_cast<int>(ingress.socketReceiveCapacityBytes());
    transport.maximumDatagramBytes = static_cast<int>(ingress.maximumDatagramBytes());
    transport.reorderWindowPackets = ingress.reorderWindowPackets();
    transport.maximumReorderDelayMs = static_cast<int>(delay.count());
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

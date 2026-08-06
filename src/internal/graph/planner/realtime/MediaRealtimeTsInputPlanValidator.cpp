#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"

namespace media::ffmpeg::graph {

::media::Status MediaRealtimeTsInputPlanValidator::validate(
    RealtimeInputType inputType, const MediaRealtimeRtpInputNodePlan& input)
{
    const bool tsInput = inputType == RealtimeInputType::MpegTsUdp;
    if (tsInput != input.mpegTs.has_value()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS input plan presence does not match input type"));
    }
    if (!tsInput) return ::media::Status::success();
    const auto& plan = *input.mpegTs;
    if (auto status = plan.validateProduct(); !status) return status;
    auto minimum = MediaRealtimeTsInputPolicy::minimumEvidenceCapacity(
        plan.packetSize, static_cast<std::uint64_t>(input.probeSizeBytes),
        plan.maximumPacketPositionRegressionBytes);
    if (!minimum) return ::media::Status::failure(minimum.error());
    if (plan.evidenceTimelineCapacity < minimum.value()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS evidence capacity is below checked minimum"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

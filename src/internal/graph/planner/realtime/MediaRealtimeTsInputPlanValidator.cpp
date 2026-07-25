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
    if (plan.demuxFormat != "mpegts" || plan.packetSize != 188 ||
        plan.avioBufferBytes == 0 || plan.maximumDatagramBytes == 0 ||
        plan.maximumDatagramBytes > plan.avioBufferBytes ||
        plan.evidenceTimelineCapacity == 0 ||
        plan.maximumPacketPositionRegressionBytes == 0 ||
        plan.pesProvenanceCapacity == 0 ||
        plan.packetOriginPolicy != MediaTsPacketOriginPolicy::PerStreamPesCarry ||
        plan.programNumber <= 0 ||
        plan.programMapPid <= 0 || plan.videoPid <= 0 || plan.audioPid <= 0 ||
        plan.pcrPid <= 0 || plan.pcrInterval27Mhz <= 0 ||
        plan.maximumPcrJitter27Mhz <= 0 || plan.maximumPcrGap27Mhz <= 0 ||
        plan.projectionCapacity == 0 ||
        plan.initialAcquiringVideoPacketCapacity == 0 ||
        plan.initialAcquiringAudioPacketCapacity == 0 ||
        plan.initialAcquiringVideoByteCapacity == 0 ||
        plan.initialAcquiringAudioByteCapacity == 0 ||
        plan.maximumAcquiringVideoPacketBytes == 0 ||
        plan.maximumAcquiringAudioPacketBytes == 0 ||
        plan.maximumAcquiringVideoPacketBytes >
            plan.initialAcquiringVideoByteCapacity ||
        plan.maximumAcquiringAudioPacketBytes >
            plan.initialAcquiringAudioByteCapacity ||
        plan.timestampTimeBaseNumerator != 1 ||
        plan.timestampTimeBaseDenominator != 90'000) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "invalid complete MPEG-TS input plan"));
    }
    auto minimum = MediaRealtimeTsInputPlan::minimumEvidenceCapacity(
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

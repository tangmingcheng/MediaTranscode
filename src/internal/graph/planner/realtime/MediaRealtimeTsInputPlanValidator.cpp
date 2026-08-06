#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"

#include <type_traits>

namespace media::ffmpeg::graph {
namespace {

bool validDuration(
    const MediaTsPacketDurationEvidence& evidence,
    const MediaTsSelectedStreamPlan& stream) noexcept
{
    return evidence.streamIndex == stream.streamIndex &&
        evidence.elementaryPid == stream.elementaryPid &&
        evidence.packetDuration > 0 && evidence.timeBase.num > 0 &&
        evidence.timeBase.den > 0 &&
        evidence.timeBase.num == stream.timeBase.num &&
        evidence.timeBase.den == stream.timeBase.den;
}

template <typename Selection>
bool validCommonSelection(const Selection& selection) noexcept
{
    return selection.programNumber > 0 && selection.programMapPid > 0 &&
        selection.programMapPid < 0x1FFF && selection.pcrPid > 0 &&
        selection.pcrPid < 0x1FFF && selection.video.streamIndex >= 0 &&
        selection.video.elementaryPid > 0 &&
        selection.video.elementaryPid < 0x1FFF &&
        selection.video.timeBase.num > 0 &&
        selection.video.timeBase.den > 0;
}

bool validSelectedProgram(const MediaTsSelectedProgramPlan& selected) noexcept
{
    return std::visit(
        [](const auto& program) {
            using Program = std::decay_t<decltype(program)>;
            if (!validCommonSelection(program.selection) ||
                !validDuration(
                    program.videoPacketDuration,
                    program.selection.video)) {
                return false;
            }
            if constexpr (std::is_same_v<
                              Program,
                              MediaTsAudioVideoSelectedProgramPlan>) {
                return program.selection.audio.streamIndex >= 0 &&
                    program.selection.audio.streamIndex !=
                        program.selection.video.streamIndex &&
                    program.selection.audio.elementaryPid > 0 &&
                    program.selection.audio.elementaryPid < 0x1FFF &&
                    program.selection.audio.elementaryPid !=
                        program.selection.video.elementaryPid &&
                    program.selection.audio.timeBase.num > 0 &&
                    program.selection.audio.timeBase.den > 0 &&
                    validDuration(
                        program.audioPacketDuration,
                        program.selection.audio);
            }
            return true;
        },
        selected);
}

bool validRetention(const MediaRealtimeTsInputPlan& plan) noexcept
{
    const bool audioVideoProgram = std::holds_alternative<
        MediaTsAudioVideoSelectedProgramPlan>(plan.selectedProgram);
    const bool audioVideoRetention = std::holds_alternative<
        MediaRealtimeTsInputPlan::AudioVideoRetention>(plan.retention);
    if (audioVideoProgram != audioVideoRetention) return false;
    return std::visit(
        [](const auto& retention) {
            using Retention = std::decay_t<decltype(retention)>;
            const bool videoValid = retention.videoPacketCapacity > 0 &&
                retention.videoByteCapacity > 0 &&
                retention.maximumVideoPacketBytes > 0 &&
                retention.maximumVideoPacketBytes <=
                    retention.videoByteCapacity;
            if constexpr (std::is_same_v<
                              Retention,
                              MediaRealtimeTsInputPlan::AudioVideoRetention>) {
                return videoValid && retention.audioPacketCapacity > 0 &&
                    retention.audioByteCapacity > 0 &&
                    retention.maximumAudioPacketBytes > 0 &&
                    retention.maximumAudioPacketBytes <=
                        retention.audioByteCapacity;
            }
            return videoValid;
        },
        plan.retention);
}

} // namespace

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
        !validSelectedProgram(plan.selectedProgram) ||
        plan.maximumPcrGap27Mhz <= 0 ||
        plan.projectionCapacity == 0 ||
        !validRetention(plan) ||
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

#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedRealtimeEmissionSet>
MediaPreparedEmissionResolver::resolve(
    const MediaPipelinePlan& videoPipeline,
    MediaRational outputFrameRate,
    const MediaAudioPipelinePlan* audioPipeline)
{
    const auto& video = videoPipeline.selected.encoder.preparedEmission;
    if (!videoPipeline.enabled ||
        videoPipeline.branchMode != MediaBranchMode::TranscodeFrame ||
        !video || video->sustainedPayloadBytesPerSecond == 0 ||
        video->peakPayloadBytesPerSecond <
            video->sustainedPayloadBytesPerSecond ||
        video->maximumAccessUnitPayloadBytes == 0 ||
        video->maximumBurstPayloadBytes == 0 ||
        video->maximumEncoderRetainedFrames == 0 ||
        !video->encodedPacketLayout ||
        video->authority.empty() || video->backend.empty() ||
        !outputFrameRate.isKnown() || outputFrameRate.num <= 0 ||
        outputFrameRate.den <= 0 ||
        video->accessUnitsPerSecondNumerator !=
            static_cast<std::uint64_t>(outputFrameRate.num) ||
        video->accessUnitsPerSecondDenominator !=
            static_cast<std::uint64_t>(outputFrameRate.den)) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::failure(
            ::media::ErrorInfo::notInitialized(
                "wire planning requires authoritative opened video emission readback"));
    }

    MediaPreparedRealtimeEmissionSet resolved{*video, std::nullopt};
    if (!audioPipeline) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::success(
            std::move(resolved));
    }
    const auto& audio = audioPipeline->preparedEmission;
    if (!audioPipeline->enabled ||
        audioPipeline->branchMode != MediaBranchMode::TranscodeFrame ||
        !audio || audio->sustainedPayloadBytesPerSecond == 0 ||
        audio->peakPayloadBytesPerSecond <
            audio->sustainedPayloadBytesPerSecond ||
        audio->maximumAccessUnitPayloadBytes == 0 ||
        audio->maximumBurstPayloadBytes == 0 ||
        audio->accessUnitsPerSecondNumerator == 0 ||
        audio->accessUnitsPerSecondDenominator == 0 ||
        audio->frameSizeSamples <= 0 || audio->authority.empty() ||
        audio->backend.empty()) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::failure(
            ::media::ErrorInfo::notInitialized(
                "wire planning requires authoritative opened audio emission readback"));
    }
    resolved.audio = *audio;
    return ::media::Result<MediaPreparedRealtimeEmissionSet>::success(
        std::move(resolved));
}

} // namespace media::ffmpeg::graph

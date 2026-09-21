#include "internal/graph/planner/capability/MediaVideoEncoderPreparer.h"

#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedVideoEncoder> MediaVideoEncoderPreparer::prepare(
    const CodecResolverEncoderContextBuildRequest& request,
    const MediaPipelineStagePlan& encoder)
{
    using Result = ::media::Result<MediaPreparedVideoEncoder>;
    if (!encoder.encoderRateControl || !encoder.preparedEmission ||
        !encoder.encodedPacketLayout || !encoder.encoderOpenContract) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "prepared output lacks its encoder emission contract"));
    }
    auto opened = CodecResolverEncoderContextBuilder::build(request);
    if (!opened) return Result::failure(opened.error());
    auto emission = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        *opened.value().context, *encoder.encoderRateControl,
        encoder.encoderOpenContract->frameRate, *encoder.encodedPacketLayout,
        "retained-output-encoder:" + encoder.ffmpegName,
        encoder.preparedEmission->backend);
    if (!emission) return Result::failure(emission.error());
    const auto& actual = emission.value();
    const auto& admitted = *encoder.preparedEmission;
    if (actual.maximumAccessUnitPayloadBytes > admitted.maximumAccessUnitPayloadBytes ||
        actual.maximumBurstPayloadBytes > admitted.maximumBurstPayloadBytes ||
        actual.maximumEncoderRetainedFrames > admitted.maximumEncoderRetainedFrames ||
        actual.peakPayloadBytesPerSecond > admitted.peakPayloadBytesPerSecond ||
        actual.sustainedPayloadBytesPerSecond != admitted.sustainedPayloadBytesPerSecond) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "retained output encoder exceeds its planned emission or retention envelope"));
    }
    auto readback = MediaVideoEncoderReadback::capture(*opened.value().context);
    if (!readback) return Result::failure(readback.error());
    if (readback.value().randomAccess != encoder.randomAccess) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Retained encoder random-access readback differs from the admitted probe"));
    }
    return Result::success({std::move(opened).value().context,
        std::move(emission).value(), std::move(readback).value()});
}

} // namespace media::ffmpeg::graph

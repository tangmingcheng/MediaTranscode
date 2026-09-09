#include "internal/graph/planner/realtime/MediaRealtimeVideoEncodingGroupContract.h"

namespace media::ffmpeg::graph {
namespace {
MediaRealtimeVideoEncodingStageContract stage(const MediaPipelineStagePlan& value)
{
    return {value.codecName, value.ffmpegName, value.filterName,
            value.hwaccelName, value.inputFrame, value.outputFrame};
}
} // namespace

::media::Result<MediaRealtimeVideoEncodingGroupContract>
MediaRealtimeVideoEncodingGroupContractPlanner::plan(
    const MediaPipelinePlan& pipeline,
    MediaNodeId sourceFanout,
    std::uint64_t sourceGeneration,
    MediaRational sourceTimeBase,
    MediaRational sourceFrameRate,
    const MediaVideoEncoderReadback& retainedEncoder)
{
    using Result = ::media::Result<MediaRealtimeVideoEncodingGroupContract>;
    const auto& chain = pipeline.selected;
    const auto& encoder = chain.encoder;
    if (!sourceFanout.isValid() || sourceGeneration == 0 ||
        !sourceTimeBase.isKnown() || !sourceFrameRate.isKnown() ||
        !pipeline.maximumFrameDuplicationGap ||
        !encoder.encoderOpenContract || !encoder.preparedEmission ||
        !encoder.encodedPacketLayout || retainedEncoder.fields.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "encoding group requires complete pipeline, source and retained encoder readback"));
    }
    auto emission = *encoder.preparedEmission;
    emission.authority.clear(); // Provenance does not alter encoder behavior.
    MediaRealtimeVideoEncodingGroupContract contract{
        sourceFanout, sourceGeneration, pipeline.sourceStreamIndex,
        stage(chain.decoder), stage(chain.filter), stage(encoder),
        *encoder.encoderOpenContract, std::move(emission),
        *encoder.encodedPacketLayout, chain.transferDirection,
        chain.decoderLineagePropagation, chain.encoderLineagePropagation,
        chain.filterImplementation, chain.encoderAbortPolicy,
        chain.filterActive, pipeline.synthesizeMissingTimestamps,
        sourceTimeBase, sourceFrameRate, *pipeline.maximumFrameDuplicationGap,
        retainedEncoder};
    return Result::success(std::move(contract));
}

} // namespace media::ffmpeg::graph

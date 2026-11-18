#include "internal/graph/planner/realtime/MediaCompositionProducerHolderPlanner.h"
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaCompositionProducerHolderFacts> MediaCompositionProducerHolderPlanner::plan(
    const MediaVideoSourcePlan& video, const MediaVideoSourceTiming& timing,
    const MediaPreparedRtpAccessUnitEnvelope& videoEnvelope,
    const MediaDecoderInputRetention* audioDecoder,
    const MediaPreparedRtpAccessUnitEnvelope* audioEnvelope)
{
    using Result = ::media::Result<MediaCompositionProducerHolderFacts>;
    if (!video.decoder.preparedInputRetention || timing.valueless_by_exception() ||
        static_cast<bool>(audioDecoder) != static_cast<bool>(audioEnvelope))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Source producer holders require opened codecs and input completion facts"));
    if (auto valid = videoEnvelope.validate(); !valid)
        return Result::failure(valid.error());
    if (audioEnvelope) {
        if (auto valid = audioEnvelope->validate(); !valid)
            return Result::failure(valid.error());
    }
    if (videoEnvelope.streamKind != MediaStreamKind::Video ||
        (audioEnvelope && audioEnvelope->streamKind != MediaStreamKind::Audio))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source producer holder envelopes differ from their stream roles"));
    if (auto valid = video.decoder.preparedInputRetention->validate(); !valid)
        return Result::failure(valid.error());
    if (audioDecoder) {
        if (auto valid = audioDecoder->validate(); !valid)
            return Result::failure(valid.error());
    }
    MediaCompositionProducerHolderFacts holders{
        {*video.decoder.preparedInputRetention, videoEnvelope.maximumAccessUnitsPerPush}, {}, {}};
    if (audioDecoder) holders.audio.emplace(MediaCompositionStreamHolderFacts{
        *audioDecoder, audioEnvelope->maximumAccessUnitsPerPush});
    if (!holders.video.maximumAtomicInputCompletion || (holders.audio && !holders.audio->maximumAtomicInputCompletion))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source producer holders require finite atomic completion bounds"));
    if (video.filterActive) {
        if (!video.filterExecution || !video.filterExecution->valid() || !video.filterExecution->inputConsumption)
            return Result::failure(::media::ErrorInfo::notInitialized("Source filter execution contract missing"));
        const auto& execution = *video.filterExecution;
        const bool canonical = std::holds_alternative<MediaObservedVideoCadence>(timing);
        if (execution.output.filterDescription != video.filter.filterName ||
            execution.frameProduct != (canonical ? MediaVideoFrameLineageProduct::CanonicalInterval
                : MediaVideoFrameLineageProduct::SourceDisplayPoint))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source filter retention differs from its planned execution"));
        holders.filterRetainedInputFrames = execution.output.maximumRetainedInputFrames;
    }
    return Result::success(std::move(holders));
}

} // namespace media::ffmpeg::graph

#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/planner/video/MediaVideoCanvasGeometryValidator.h"

namespace media::ffmpeg::graph {
::media::Status MediaAvContinuousAggregatePlanValidator::validate(const MediaAvContinuousAggregateTopology& plan)
{
    const auto invalid = [](const char* message) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
    };
    if (!plan.outputGroupKey.valid() || plan.sources.empty() || plan.audioSource >= plan.sources.size() ||
        plan.canvas.tiles.size() != plan.sources.size() || plan.audioPort.empty() ||
        plan.videoFrameRate.num <= 0 || plan.videoFrameRate.den <= 0 ||
        plan.audio.sampleRate() <= 0 || plan.audio.codecFrameSamples() <= 0 ||
        plan.initialGeneration == 0 || plan.initialAudioSample < 0 ||
        plan.maximumAudioCandidates == 0 || plan.maximumAudioCandidateSamples <= 0 ||
        plan.maximumAudioContributions == 0 || plan.maximumMetadataBytes == 0 ||
        plan.preparationLead.nanoseconds() <= 0)
        return invalid("Continuous aggregate requires its complete planner product");
    if (auto status = MediaVideoCanvasGeometryValidator::validate(plan.canvas); !status) return status;
    for (std::size_t i = 0; i < plan.sources.size(); ++i) {
        const auto& source = plan.sources[i];
        if (!source.groupKey.valid() || source.groupKey == plan.outputGroupKey ||
            source.videoPort.empty() || source.maximumVideoCandidates == 0 ||
            (source.discardedAudioPort && (i == plan.audioSource || source.discardedAudioPort->empty())))
            return invalid("Continuous aggregate source identity or capacity differs from its plan");
        for (std::size_t previous = 0; previous < i; ++previous)
            if (plan.sources[previous].groupKey == source.groupKey)
                return invalid("Aggregate source domains must be distinct");
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph

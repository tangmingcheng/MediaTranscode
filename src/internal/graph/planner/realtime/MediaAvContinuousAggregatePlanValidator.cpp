#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/planner/video/MediaVideoCanvasGeometryValidator.h"
#include "internal/graph/planner/video/MediaVideoColorContractPlanner.h"

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
        plan.maximumAudioContributions != static_cast<std::size_t>(plan.audio.codecFrameSamples()) ||
        plan.storage.fixedStorageBytes == 0 || plan.storage.retainedMetadataBytes == 0 ||
        plan.storage.maximumRetainedObjects == 0 ||
        plan.storage.fixedStorageBytes > plan.maximumMetadataBytes ||
        plan.storage.retainedMetadataBytes != plan.maximumMetadataBytes - plan.storage.fixedStorageBytes ||
        plan.preparationLead.nanoseconds() <= 0)
        return invalid("Continuous aggregate requires its complete planner product");
    if (auto status = MediaVideoCanvasGeometryValidator::validate(plan.canvas); !status) return status;
    if (!plan.canvas.effectiveColorRange.color)
        return invalid("Aggregate canvas lacks its full prepared color contract");
    if (auto status = MediaVideoColorContractPlanner::validate(*plan.canvas.effectiveColorRange.color); !status)
        return status;
    for (std::size_t i = 0; i < plan.sources.size(); ++i) {
        const auto& source = plan.sources[i];
        if (!source.groupKey.valid() || source.groupKey == plan.outputGroupKey ||
            source.videoPort.empty() || source.maximumVideoCandidates == 0 ||
            source.videoIdentity.empty() ||
            (source.videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint && source.maximumVideoCandidates < 2) ||
            (source.videoProduct != MediaVideoFrameLineageProduct::CanonicalInterval &&
             source.videoProduct != MediaVideoFrameLineageProduct::SourceDisplayPoint) ||
            (source.audioIdentity && source.audioIdentity->empty()) ||
            (i == plan.audioSource && !source.audioIdentity) ||
            source.discardedAudioPort.has_value() != (i != plan.audioSource && source.audioIdentity.has_value()) ||
            (source.discardedAudioPort && source.discardedAudioPort->empty()))
            return invalid("Continuous aggregate source identity or capacity differs from its plan");
        for (std::size_t previous = 0; previous < i; ++previous)
            if (plan.sources[previous].groupKey == source.groupKey)
                return invalid("Aggregate source domains must be distinct");
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph

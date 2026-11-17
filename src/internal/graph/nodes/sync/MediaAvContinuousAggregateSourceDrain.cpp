#include "internal/graph/nodes/sync/MediaAggregateSourceDrain.h"
#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"

namespace media::ffmpeg::graph {
::media::Status MediaAvContinuousAggregateNode::serviceSourceDrains()
{
    // Called by onProcess only, after base deferred transfers release commit permits.
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        auto& group = *m_dependencies.sources[i].group;
        if (source.videoClosingRetired || source.discardedAudioClosingRetired) continue;
        auto closing = group.sourceDrainSnapshot();
        if (!source.videoClosingGeneration && !source.discardedAudioClosingGeneration) continue;
        if (!closing) {
            const auto retired = source.videoClosingGeneration ? *source.videoClosingGeneration
                : *source.discardedAudioClosingGeneration;
            if (source.videoClosingGeneration && source.discardedAudioClosingGeneration &&
                *source.videoClosingGeneration != *source.discardedAudioClosingGeneration)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Aggregate source members disagree on their closing generation"));
            if (mediaAggregateSourceDrainRetired(group, *source.purge, retired)) continue;
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Aggregate source lost its closing generation before purge"));
        }
        const auto generation = closing->request.playbackGeneration;
        if ((source.videoClosingGeneration && *source.videoClosingGeneration != generation) ||
            (source.discardedAudioClosingGeneration && *source.discardedAudioClosingGeneration != generation))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Aggregate source drain has inconsistent member generations"));
        const bool pendingContribution = m_pending && m_pending->sourceGenerations[i] == generation;
        if (source.videoClosingGeneration && !pendingContribution) {
            if (plan().sources[i].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint &&
                source.video.size() == 1 && source.lastDisplayCommit) {
                const auto* last = dynamic_cast<const MediaVideoDecodeBuffer*>(source.video.front().get());
                const auto display = last ? last->displayLineage() : nullptr;
                if (display && display->accessUnit && display->accessUnit->generation == generation &&
                    last->decodedSequence() == source.lastDisplayCommit->decodedSequence)
                    source.video.pop_front();
            }
            if (source.video.empty() && !closing->video.drained) {
                auto acknowledged = group.acknowledgeSourceDrained(generation, MediaStreamKind::Video);
                if (!acknowledged) {
                    if (mediaAggregateSourceDrainRetired(group, *source.purge, generation)) continue;
                    return ::media::Status::failure(acknowledged.error());
                }
            }
        }
        if (source.discardedAudioClosingGeneration && closing->audio && !closing->audio->drained) {
            // The discard owner holds no audio or deferred audio transaction at this boundary.
            auto acknowledged = group.acknowledgeSourceDrained(generation, MediaStreamKind::Audio);
            if (!acknowledged) {
                if (mediaAggregateSourceDrainRetired(group, *source.purge, generation)) continue;
                return ::media::Status::failure(acknowledged.error());
            }
        }
        closing = group.sourceDrainSnapshot();
        if (closing && closing->request.playbackGeneration == generation && closing->complete()) {
            if (auto status = group.completeSourceDrain(generation); !status) {
                if (mediaAggregateSourceDrainRetired(group, *source.purge, generation)) continue;
                return status;
            }
        }
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph

#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include <sstream>

namespace media::ffmpeg::graph {
void MediaAvOutputSchedulerNode::observeVideoProgressRelease() noexcept
{
    if (m_generationData->firstVideoProgressReleaseObserved) return;
    m_generationData->firstVideoProgressReleaseObserved = true;
    try {
        const auto& progress = m_generationData->videoProgress;
        const auto& audio = m_generationData->audioHead;
        if (!progress || !audio) return;
        const auto dispatch = audio->canonicalDispatchTime();
        if (!dispatch) return;
        std::ostringstream out;
        out << "av_scheduler_video_progress_release node=" << nodeId().value
            << " generation=" << progress->generation
            << " audio_sequence=" << audio->canonical()->canonicalSequence().value()
            << " audio_canonical_dispatch_ns=" << dispatch.value().nanoseconds()
            << " video_canonical_lower_bound_ns=" << progress->followingAccessUnitsLowerBound.nanoseconds()
            << " observed_after_output_commit=1";
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {}
}

::media::Result<bool> MediaAvOutputSchedulerNode::audioPrecedesVideoProgress() const
{
    using Result = ::media::Result<bool>;
    const auto& progress = m_generationData->videoProgress;
    const auto& audio = m_generationData->audioHead;
    if (!progress || !audio || !m_generationData->activeGeneration ||
        progress->generation != *m_generationData->activeGeneration ||
        audio->generation() != progress->generation) return Result::success(false);
    auto dispatch = audio->canonicalDispatchTime();
    if (!dispatch) return Result::failure(dispatch.error());
    auto audioMaster = m_group->mapCanonicalToMaster(dispatch.value());
    if (!audioMaster) return Result::failure(audioMaster.error());
    auto boundMaster = m_group->mapCanonicalToMaster(progress->followingAccessUnitsLowerBound);
    if (!boundMaster) return Result::failure(boundMaster.error());
    return Result::success(audioMaster.value() < boundMaster.value());
}

::media::Result<std::optional<MediaAvSchedulerVideoProgress>>
MediaAvOutputSchedulerNode::prepareVideoProgress(const MediaCanonicalAccessUnitBuffer& unit) const
{
    using Result = ::media::Result<std::optional<MediaAvSchedulerVideoProgress>>;
    if (unit.stream() != MediaScheduledStream::Video) return Result::success(std::nullopt);
    const auto& previous = m_generationData->videoProgress;
    auto dispatch = unit.canonicalDispatch();
    if (!dispatch) return Result::failure(dispatch.error());
    if (previous && (previous->generation != unit.generation() ||
        !sameMediaCanonicalTimelineIdentity(previous->timeline, unit.lineage()->identity) ||
        dispatch.value() < previous->followingAccessUnitsLowerBound ||
        (unit.decodeProgress() && unit.decodeProgress()->followingAccessUnitsLowerBound <
            previous->followingAccessUnitsLowerBound)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video AU contradicts committed encoded progress"));
    if (!unit.decodeProgress()) return Result::success(previous);
    return Result::success(MediaAvSchedulerVideoProgress{
        unit.generation(), unit.lineage()->identity, unit.decodeProgress()->followingAccessUnitsLowerBound});
}
}

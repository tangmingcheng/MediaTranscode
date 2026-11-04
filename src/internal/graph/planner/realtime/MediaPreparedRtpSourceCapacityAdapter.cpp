#include "internal/graph/planner/realtime/MediaPreparedRtpSourceCapacityAdapter.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmission.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
namespace media::ffmpeg::graph {
::media::Result<MediaAvSourceCapacityFacts> MediaPreparedRtpSourceCapacityAdapter::read(
    const MediaPreparedRtpStorageAdmission& admission,const MediaPreparationMemoryResource& owner,
    MediaTranscodeStreamSet members)
{
    using Result=::media::Result<MediaAvSourceCapacityFacts>;
    const bool audio=members==MediaTranscodeStreamSet::AudioVideo;
    if((!audio && members!=MediaTranscodeStreamSet::VideoOnly) || !admission.owner || admission.owner.get()!=&owner ||
        !admission.totalBytes || admission.totalBytes>admission.sharedCapacityBytes || admission.acquisitionWindow.nanoseconds()<=0 ||
        admission.audio.has_value()!=audio ||
        admission.route.shape.members!=members || admission.route.shape.readiness!=MediaVideoStartupReadiness::DecodedPresentationSpan ||
        !admission.video.bounds.acquiringObjects || (audio && !admission.audio->bounds.acquiringObjects))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source capacity requires matching sealed owner, budget and route admission"));
    auto route=MediaStartupRoutePlanner::bind(admission.route.shape,admission.route.metadata,admission.route.atomicMetadata);
    if(!route)return Result::failure(route.error());
    return Result::success({admission.startup,std::move(route).value(),admission.queues,
        admission.video.bounds.acquiringObjects,audio?std::optional{admission.audio->bounds.acquiringObjects}:std::nullopt,
        admission.acquisitionWindow});
}
}

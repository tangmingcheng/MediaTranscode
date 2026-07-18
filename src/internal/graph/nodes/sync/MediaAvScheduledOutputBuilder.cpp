#include "internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.h"

#include "internal/graph/sync/MediaScheduledPayloadClone.h"

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedScheduledOutput>
MediaAvScheduledOutputBuilder::canonicalVideo(
    const MediaAvSchedulerHead& head,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaVideoSyncDecisionKind decision)
{
    const auto* unit = head.canonical();
    if (!unit) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical video output requires a canonical head"));
    }
    auto canonicalDispatch = unit->canonicalDispatch();
    if (!canonicalDispatch) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(
            canonicalDispatch.error());
    }
    auto displayedClone = MediaScheduledPayloadClone::clonePacket(unit->media());
    if (!displayedClone) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(
            displayedClone.error());
    }
    MediaScheduledAccessUnitParameters parameters{
        unit->media(), MediaScheduledStream::Video,
        unit->canonicalPresentation(), canonicalDispatch.value(),
        presentationOnMaster, dispatchOnMaster, emitOnMaster,
        unit->canonicalDuration(),
        unit->generation(), unit->sourceSequence(), std::nullopt, std::nullopt,
        decision};
    auto output = MediaScheduledAccessUnit::create(std::move(parameters));
    if (!output) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(
            output.error());
    }
    return ::media::Result<MediaPreparedScheduledOutput>::success(
        MediaPreparedScheduledOutput{
            std::move(output).value(), std::move(displayedClone).value()});
}

::media::Result<MediaPreparedScheduledOutput>
MediaAvScheduledOutputBuilder::repeatedVideo(
    const MediaVideoRepeatRequestBuffer& repeat,
    const MediaBufferRef& lastDisplayedVideo,
    MediaSourceAccessUnitSequence lastDisplayedSequence,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaVideoSyncDecisionKind decision)
{
    if (!lastDisplayedVideo) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(
            ::media::ErrorInfo::notInitialized(
                "Repeated video output requires a repeat head and displayed frame"));
    }
    auto media = MediaScheduledPayloadClone::clonePacket(lastDisplayedVideo);
    if (!media) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(media.error());
    }
    MediaScheduledAccessUnitParameters parameters{
        std::move(media).value(), MediaScheduledStream::Video,
        repeat.canonicalPresentation(), repeat.canonicalPresentation(),
        presentationOnMaster, dispatchOnMaster, emitOnMaster,
        repeat.canonicalDuration(),
        repeat.generation(), lastDisplayedSequence, lastDisplayedSequence,
        repeat.requestId(), decision};
    auto output = MediaScheduledAccessUnit::create(std::move(parameters));
    if (!output) {
        return ::media::Result<MediaPreparedScheduledOutput>::failure(output.error());
    }
    return ::media::Result<MediaPreparedScheduledOutput>::success(
        MediaPreparedScheduledOutput{std::move(output).value(), {}});
}

::media::Result<MediaBufferRef> MediaAvScheduledOutputBuilder::audio(
    const MediaCanonicalAccessUnitBuffer& unit,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster)
{
    auto canonicalDispatch = unit.canonicalDispatch();
    if (!canonicalDispatch) return ::media::Result<MediaBufferRef>::failure(
        canonicalDispatch.error());
    return MediaScheduledAccessUnit::create(MediaScheduledAccessUnitParameters{
        unit.media(), MediaScheduledStream::Audio,
        unit.canonicalPresentation(), canonicalDispatch.value(),
        presentationOnMaster, dispatchOnMaster, emitOnMaster,
        unit.canonicalDuration(),
        unit.generation(), unit.sourceSequence(), std::nullopt, std::nullopt,
        std::nullopt});
}

} // namespace media::ffmpeg::graph

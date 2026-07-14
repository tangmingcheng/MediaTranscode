#pragma once

#include "internal/graph/nodes/sync/MediaAvSchedulerHead.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaPreparedScheduledOutput final {
    MediaBufferRef output;
    MediaBufferRef displayedVideoClone;
};

class MediaAvScheduledOutputBuilder final {
public:
    static ::media::Result<MediaPreparedScheduledOutput> canonicalVideo(
        const MediaAvSchedulerHead& head,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaVideoSyncDecisionKind decision);
    static ::media::Result<MediaPreparedScheduledOutput> repeatedVideo(
        const MediaVideoRepeatRequestBuffer& repeat,
        const MediaBufferRef& lastDisplayedVideo,
        MediaSourceAccessUnitSequence lastDisplayedSequence,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaVideoSyncDecisionKind decision);
    static ::media::Result<MediaBufferRef> audio(
        const MediaCanonicalAccessUnitBuffer& unit,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster);
};

} // namespace media::ffmpeg::graph

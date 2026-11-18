#pragma once

#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncRuntimePlan;
struct MediaRealtimeAvSourceRuntimePlan;

MediaAudioPacketCopyBranchOptions makeAudioPacketCopyBranchOptions(const MediaAudioBranchSegmentOptions& options);
MediaAudioEncodeBranchOptions makeAudioEncodeBranchOptions(const MediaAudioBranchSegmentOptions& options);
::media::Status mapSynchronizedAudioBranchOptions(
    const MediaRealtimeAvSyncRuntimePlan& runtime,
    MediaAudioBranchSegmentOptions& options);

::media::Status mapSynchronizedAudioSourceOptions(
    const MediaRealtimeAvSourceRuntimePlan& runtime,
    MediaAudioBranchSegmentOptions& options);

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaBranchEndpointValidator.h"

namespace media::ffmpeg::graph {

MediaBranchEndpointSet makeAudioBranchEndpointSet(const MediaAudioBranchSegmentOptions& options);
MediaAudioPacketCopyBranchOptions makeAudioPacketCopyBranchOptions(const MediaAudioBranchSegmentOptions& options);
MediaAudioEncodeBranchOptions makeAudioEncodeBranchOptions(const MediaAudioBranchSegmentOptions& options);

} // namespace media::ffmpeg::graph

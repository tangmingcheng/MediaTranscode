#pragma once

#include "internal/graph/builder/segments/MediaBranchEndpointValidator.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

namespace media::ffmpeg::graph {

MediaBranchEndpointSet makeVideoBranchEndpointSet(const MediaVideoBranchSegmentOptions& options);
MediaVideoPacketCopyBranchOptions makeVideoPacketCopyBranchOptions(const MediaVideoBranchSegmentOptions& options);
MediaVideoTranscodeBranchOptions makeVideoTranscodeBranchOptions(const MediaVideoBranchSegmentOptions& options);

} // namespace media::ffmpeg::graph

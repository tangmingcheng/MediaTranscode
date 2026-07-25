#pragma once

#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

namespace media::ffmpeg::graph {

MediaVideoPacketCopyBranchOptions makeVideoPacketCopyBranchOptions(const MediaVideoBranchSegmentOptions& options);
MediaVideoTranscodeBranchOptions makeVideoTranscodeBranchOptions(const MediaVideoBranchSegmentOptions& options);

} // namespace media::ffmpeg::graph

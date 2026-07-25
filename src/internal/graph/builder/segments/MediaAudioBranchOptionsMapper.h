#pragma once

#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"

namespace media::ffmpeg::graph {

MediaAudioPacketCopyBranchOptions makeAudioPacketCopyBranchOptions(const MediaAudioBranchSegmentOptions& options);
MediaAudioEncodeBranchOptions makeAudioEncodeBranchOptions(const MediaAudioBranchSegmentOptions& options);

} // namespace media::ffmpeg::graph

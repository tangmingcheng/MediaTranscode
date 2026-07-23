#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"

#include "internal/graph/builder/segments/MediaVideoBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaEncodedBranchEndpoints> buildPlannedVideoBranch(
    MediaGraph& graph,
    const MediaVideoBranchSegmentOptions& options)
{
    switch (options.plan.branchMode) {
    case MediaBranchMode::CopyPacket:
        return MediaVideoPacketCopyBranchBuilder::build(graph, makeVideoPacketCopyBranchOptions(options));
    case MediaBranchMode::TranscodeFrame:
        return MediaVideoTranscodeBranchBuilder::build(graph, makeVideoTranscodeBranchOptions(options));
    case MediaBranchMode::Drop:
        break;
    }

    return ::media::Result<MediaEncodedBranchEndpoints>::failure(
        ::media::ErrorInfo::unsupported("MediaVideoBranchSegmentBuilder unsupported video branch mode"));
}

} // namespace

::media::Result<MediaEncodedBranchEndpoints> MediaVideoBranchSegmentBuilder::build(
    MediaGraph& graph,
    const MediaVideoBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaVideoBranchSegmentBuilder requires an enabled video branch"));
    }

    return buildPlannedVideoBranch(graph, options);
}

} // namespace media::ffmpeg::graph

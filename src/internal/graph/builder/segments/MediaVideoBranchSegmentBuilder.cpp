#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"

#include "internal/graph/builder/segments/MediaBranchEndpointValidator.h"
#include "internal/graph/builder/segments/MediaVideoBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaVideoBranchSegmentBuilder";

::media::Result<void> buildPlannedVideoBranch(MediaGraph& graph,
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

    return ::media::Result<void>::failure(
        ::media::ErrorInfo::unsupported("MediaVideoBranchSegmentBuilder unsupported video branch mode"));
}

} // namespace

::media::Result<bool> MediaVideoBranchSegmentBuilder::buildIfPlanned(
    MediaGraph& graph,
    const MediaVideoBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<bool>::success(false);
    }

    if (auto status = validateMediaBranchEndpoints(owner, makeVideoBranchEndpointSet(options)); !status) {
        return ::media::Result<bool>::failure(status.error());
    }

    if (auto status = buildPlannedVideoBranch(graph, options); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph

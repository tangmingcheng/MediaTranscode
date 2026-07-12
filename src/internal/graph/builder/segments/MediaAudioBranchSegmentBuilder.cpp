#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"

#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaBranchEndpointValidator.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioBranchSegmentBuilder";

::media::Result<void> buildPlannedAudioBranch(MediaGraph& graph,
                                              const MediaAudioBranchSegmentOptions& options)
{
    switch (options.plan.branchMode) {
    case MediaBranchMode::CopyPacket:
        return MediaAudioPacketCopyBranchBuilder::build(graph, makeAudioPacketCopyBranchOptions(options));
    case MediaBranchMode::TranscodeFrame:
        return MediaAudioEncodeBranchBuilder::build(graph, makeAudioEncodeBranchOptions(options));
    case MediaBranchMode::Drop:
        break;
    }

    return ::media::Result<void>::failure(
        ::media::ErrorInfo::unsupported("MediaAudioBranchSegmentBuilder unsupported audio branch mode"));
}

} // namespace

::media::Result<bool> MediaAudioBranchSegmentBuilder::buildIfPlanned(
    MediaGraph& graph,
    const MediaAudioBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<bool>::success(false);
    }
    if (!options.normalizeInputPackets.has_value()) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires explicit input packet normalization policy"));
    }

    if (auto status = validateMediaBranchEndpoints(owner, makeAudioBranchEndpointSet(options)); !status) {
        return ::media::Result<bool>::failure(status.error());
    }

    if (auto status = buildPlannedAudioBranch(graph, options); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph

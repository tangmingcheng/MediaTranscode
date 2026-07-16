#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"

#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaAudioBranchSegmentResult> buildPlannedAudioBranch(
    MediaGraph& graph,
    const MediaAudioBranchSegmentOptions& options)
{
    switch (options.plan.branchMode) {
    case MediaBranchMode::CopyPacket:
    {
        auto copy = MediaAudioPacketCopyBranchBuilder::build(
            graph, makeAudioPacketCopyBranchOptions(options));
        if (!copy) {
            return ::media::Result<MediaAudioBranchSegmentResult>::failure(
                copy.error());
        }
        return ::media::Result<MediaAudioBranchSegmentResult>::success(
            {true, copy.value().codec, copy.value().packet});
    }
    case MediaBranchMode::TranscodeFrame:
    {
        auto encode = MediaAudioEncodeBranchBuilder::build(
            graph, makeAudioEncodeBranchOptions(options));
        if (!encode) {
            return ::media::Result<MediaAudioBranchSegmentResult>::failure(
                encode.error());
        }
        return ::media::Result<MediaAudioBranchSegmentResult>::success(
            {true, encode.value().codec, encode.value().packet});
    }
    case MediaBranchMode::Drop:
        break;
    }

    return ::media::Result<MediaAudioBranchSegmentResult>::failure(
        ::media::ErrorInfo::unsupported("MediaAudioBranchSegmentBuilder unsupported audio branch mode"));
}

} // namespace

::media::Result<MediaAudioBranchSegmentResult>
MediaAudioBranchSegmentBuilder::buildIfPlanned(
    MediaGraph& graph,
    const MediaAudioBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<MediaAudioBranchSegmentResult>::success({});
    }
    if (!options.normalizeInputPackets.has_value()) {
        return ::media::Result<MediaAudioBranchSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires explicit input packet normalization policy"));
    }
    if (!options.correctionMode || !options.lineageMode) {
        return ::media::Result<MediaAudioBranchSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioBranchSegmentBuilder requires explicit correction and lineage modes"));
    }
    if (options.plan.branchMode == MediaBranchMode::CopyPacket &&
        (*options.correctionMode !=
             MediaAudioCorrectionExecutionMode::Disabled ||
         *options.lineageMode !=
             MediaAudioLineageExecutionMode::LegacyPlainPacket ||
         options.lineageCapacity || options.correctionGeneration ||
         options.correctionLookaheadWindows || options.syncGroup)) {
        return ::media::Result<MediaAudioBranchSegmentResult>::failure(
            ::media::ErrorInfo::unsupported(
                "synchronized audio packet copy is unsupported"));
    }

    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty() ||
        !options.packetSourceNode.isValid() || options.packetSourcePort.empty() ||
        options.queues.metadata == 0 || options.queues.packet == 0 ||
        options.queues.frame == 0) {
        return ::media::Result<MediaAudioBranchSegmentResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioBranchSegmentBuilder requires planned source endpoints and queues"));
    }

    return buildPlannedAudioBranch(graph, options);
}

} // namespace media::ffmpeg::graph

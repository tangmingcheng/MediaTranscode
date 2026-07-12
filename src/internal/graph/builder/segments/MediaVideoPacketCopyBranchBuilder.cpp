#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"

#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

namespace media::ffmpeg::graph {

::media::Result<void> MediaVideoPacketCopyBranchBuilder::build(
    MediaGraph& graph,
    const MediaVideoPacketCopyBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::CopyPacket) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("video packet copy branch requires CopyPacket mode"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("video packet copy branch requires a planned source stream index"));
    }
    if (!options.normalizePackets.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("video packet copy branch requires explicit normalization policy"));
    }

    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = options.prefix;
    branchOptions.streamKind = MediaStreamKind::Video;
    branchOptions.sourceStreamIndex = options.plan.sourceStreamIndex;
    branchOptions.formatSourceNode = options.formatSourceNode;
    branchOptions.formatSourcePort = options.formatSourcePort;
    branchOptions.packetSourceNode = options.packetSourceNode;
    branchOptions.packetSourcePort = options.packetSourcePort;
    branchOptions.muxNode = options.muxNode;
    branchOptions.muxCodecPort = options.muxCodecPort;
    branchOptions.muxPacketPort = options.muxPacketPort;
    branchOptions.normalizePackets = options.normalizePackets;
    branchOptions.queues = options.queues;
    branchOptions.edgePolicies = options.edgePolicies;
    return MediaPacketCopyBranchBuilder::build(graph, branchOptions);
}

} // namespace media::ffmpeg::graph

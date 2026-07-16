#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"

#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"

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
    branchOptions.normalizePackets = options.normalizePackets;
    branchOptions.queues = options.queues;
    branchOptions.edgePolicies = options.edgePolicies;
    auto branch = MediaPacketCopyBranchBuilder::build(graph, branchOptions);
    if (!branch) return ::media::Result<void>::failure(branch.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, "MediaVideoPacketCopyBranchBuilder",
            branch.value().codec.node, branch.value().codec.port,
            options.muxNode, options.muxCodecPort,
            options.prefix + ".codec -> mux.codec",
            options.edgePolicies.metadata); !status) return status;
    return MediaGraphBuildSupport::connectChecked(
        graph, "MediaVideoPacketCopyBranchBuilder",
        branch.value().packet.node, branch.value().packet.port,
        options.muxNode, options.muxPacketPort,
        options.prefix + ".packet -> mux.packet",
        options.edgePolicies.videoMux);
}

} // namespace media::ffmpeg::graph

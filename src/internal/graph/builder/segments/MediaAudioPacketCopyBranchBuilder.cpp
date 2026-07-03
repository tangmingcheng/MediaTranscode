#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"

#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

namespace media::ffmpeg::graph {

::media::Result<void> MediaAudioPacketCopyBranchBuilder::build(
    MediaGraph& graph,
    const MediaAudioPacketCopyBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::CopyPacket) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("audio packet copy branch requires CopyPacket mode"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("audio packet copy branch requires a planned source stream index"));
    }

    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = options.prefix;
    branchOptions.streamKind = MediaStreamKind::Audio;
    branchOptions.sourceStreamIndex = options.plan.sourceStreamIndex;
    branchOptions.formatSourceNode = options.formatSourceNode;
    branchOptions.formatSourcePort = options.formatSourcePort;
    branchOptions.packetSourceNode = options.packetSourceNode;
    branchOptions.packetSourcePort = options.packetSourcePort;
    branchOptions.muxNode = options.muxNode;
    branchOptions.muxCodecPort = options.muxCodecPort;
    branchOptions.muxPacketPort = options.muxPacketPort;
    branchOptions.queues = options.queues;
    return MediaPacketCopyBranchBuilder::build(graph, branchOptions);
}

} // namespace media::ffmpeg::graph

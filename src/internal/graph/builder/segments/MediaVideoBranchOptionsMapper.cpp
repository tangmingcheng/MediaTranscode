#include "internal/graph/builder/segments/MediaVideoBranchOptionsMapper.h"

namespace media::ffmpeg::graph {

MediaVideoPacketCopyBranchOptions makeVideoPacketCopyBranchOptions(const MediaVideoBranchSegmentOptions& options)
{
    MediaVideoPacketCopyBranchOptions copyOptions;
    copyOptions.prefix = options.prefix + ".copy";
    copyOptions.plan = options.plan;
    copyOptions.queues = options.queues;
    copyOptions.edgePolicies = options.edgePolicies;
    copyOptions.formatSourceNode = options.formatSourceNode;
    copyOptions.formatSourcePort = options.formatSourcePort;
    copyOptions.packetSourceNode = options.packetSourceNode;
    copyOptions.packetSourcePort = options.packetSourcePort;
    copyOptions.normalizePackets = options.normalizePacketCopy;
    return copyOptions;
}

MediaVideoTranscodeBranchOptions makeVideoTranscodeBranchOptions(const MediaVideoBranchSegmentOptions& options)
{
    MediaVideoTranscodeBranchOptions transcodeOptions;
    transcodeOptions.prefix = options.prefix + ".transcode";
    transcodeOptions.plan = options.plan;
    transcodeOptions.parameters = options.parameters;
    transcodeOptions.queues = options.queues;
    transcodeOptions.edgePolicies = options.edgePolicies;
    transcodeOptions.inputStartRequiresKeyFrame = options.inputStartRequiresKeyFrame;
    transcodeOptions.canonicalLineageCapacity = options.canonicalLineageCapacity;
    transcodeOptions.generationStartRequiresKeyFrame =
        options.generationStartRequiresKeyFrame;
    transcodeOptions.formatSourceNode = options.formatSourceNode;
    transcodeOptions.formatSourcePort = options.formatSourcePort;
    transcodeOptions.packetSourceNode = options.packetSourceNode;
    transcodeOptions.packetSourcePort = options.packetSourcePort;
    return transcodeOptions;
}

} // namespace media::ffmpeg::graph

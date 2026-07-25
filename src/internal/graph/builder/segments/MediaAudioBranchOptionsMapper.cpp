#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"

namespace media::ffmpeg::graph {

MediaAudioPacketCopyBranchOptions makeAudioPacketCopyBranchOptions(const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioPacketCopyBranchOptions copyOptions;
    copyOptions.prefix = options.prefix + ".copy";
    copyOptions.plan = options.plan;
    copyOptions.queues = options.queues;
    copyOptions.edgePolicies = options.edgePolicies;
    copyOptions.formatSourceNode = options.formatSourceNode;
    copyOptions.formatSourcePort = options.formatSourcePort;
    copyOptions.packetSourceNode = options.packetSourceNode;
    copyOptions.packetSourcePort = options.packetSourcePort;
    copyOptions.normalizePackets = options.normalizeInputPackets;
    return copyOptions;
}

MediaAudioEncodeBranchOptions makeAudioEncodeBranchOptions(const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioEncodeBranchOptions encodeOptions;
    encodeOptions.prefix = options.prefix + ".encode";
    encodeOptions.plan = options.plan;
    encodeOptions.queues = options.queues;
    encodeOptions.edgePolicies = options.edgePolicies;
    encodeOptions.formatSourceNode = options.formatSourceNode;
    encodeOptions.formatSourcePort = options.formatSourcePort;
    encodeOptions.packetSourceNode = options.packetSourceNode;
    encodeOptions.packetSourcePort = options.packetSourcePort;
    encodeOptions.normalizePackets = options.normalizeInputPackets;
    encodeOptions.correctionMode = options.correctionMode;
    encodeOptions.lineageMode = options.lineageMode;
    encodeOptions.lineageCapacity = options.lineageCapacity;
    encodeOptions.correctionGeneration = options.correctionGeneration;
    encodeOptions.correctionLookaheadWindows = options.correctionLookaheadWindows;
    encodeOptions.syncGroup = options.syncGroup;
    return encodeOptions;
}

} // namespace media::ffmpeg::graph

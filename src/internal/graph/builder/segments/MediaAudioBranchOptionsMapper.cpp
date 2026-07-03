#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"

namespace media::ffmpeg::graph {

MediaBranchEndpointSet makeAudioBranchEndpointSet(const MediaAudioBranchSegmentOptions& options)
{
    MediaBranchEndpointSet endpoints;
    endpoints.formatSourceNode = options.formatSourceNode;
    endpoints.formatSourcePort = options.formatSourcePort;
    endpoints.packetSourceNode = options.packetSourceNode;
    endpoints.packetSourcePort = options.packetSourcePort;
    endpoints.muxNode = options.muxNode;
    endpoints.muxCodecPort = options.muxCodecPort;
    endpoints.muxPacketPort = options.muxPacketPort;
    endpoints.queues = options.queues;
    return endpoints;
}

MediaAudioPacketCopyBranchOptions makeAudioPacketCopyBranchOptions(const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioPacketCopyBranchOptions copyOptions;
    copyOptions.prefix = options.prefix + ".copy";
    copyOptions.plan = options.plan;
    copyOptions.queues = options.queues;
    copyOptions.formatSourceNode = options.formatSourceNode;
    copyOptions.formatSourcePort = options.formatSourcePort;
    copyOptions.packetSourceNode = options.packetSourceNode;
    copyOptions.packetSourcePort = options.packetSourcePort;
    copyOptions.muxNode = options.muxNode;
    copyOptions.muxCodecPort = options.muxCodecPort;
    copyOptions.muxPacketPort = options.muxPacketPort;
    return copyOptions;
}

MediaAudioEncodeBranchOptions makeAudioEncodeBranchOptions(const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioEncodeBranchOptions encodeOptions;
    encodeOptions.prefix = options.prefix + ".encode";
    encodeOptions.plan = options.plan;
    encodeOptions.parameters = options.parameters;
    encodeOptions.queues = options.queues;
    encodeOptions.formatSourceNode = options.formatSourceNode;
    encodeOptions.formatSourcePort = options.formatSourcePort;
    encodeOptions.packetSourceNode = options.packetSourceNode;
    encodeOptions.packetSourcePort = options.packetSourcePort;
    encodeOptions.muxNode = options.muxNode;
    encodeOptions.muxCodecPort = options.muxCodecPort;
    encodeOptions.muxPacketPort = options.muxPacketPort;
    return encodeOptions;
}

} // namespace media::ffmpeg::graph

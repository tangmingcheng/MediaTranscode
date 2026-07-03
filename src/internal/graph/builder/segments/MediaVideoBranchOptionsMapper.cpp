#include "internal/graph/builder/segments/MediaVideoBranchOptionsMapper.h"

namespace media::ffmpeg::graph {

MediaBranchEndpointSet makeVideoBranchEndpointSet(const MediaVideoBranchSegmentOptions& options)
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

MediaVideoPacketCopyBranchOptions makeVideoPacketCopyBranchOptions(const MediaVideoBranchSegmentOptions& options)
{
    MediaVideoPacketCopyBranchOptions copyOptions;
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

MediaVideoTranscodeBranchOptions makeVideoTranscodeBranchOptions(const MediaVideoBranchSegmentOptions& options)
{
    MediaVideoTranscodeBranchOptions transcodeOptions;
    transcodeOptions.prefix = options.prefix + ".transcode";
    transcodeOptions.plan = options.plan;
    transcodeOptions.parameters = options.parameters;
    transcodeOptions.queues = options.queues;
    transcodeOptions.formatSourceNode = options.formatSourceNode;
    transcodeOptions.formatSourcePort = options.formatSourcePort;
    transcodeOptions.packetSourceNode = options.packetSourceNode;
    transcodeOptions.packetSourcePort = options.packetSourcePort;
    transcodeOptions.muxNode = options.muxNode;
    transcodeOptions.muxCodecPort = options.muxCodecPort;
    transcodeOptions.muxPacketPort = options.muxPacketPort;
    return transcodeOptions;
}

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaBranchEndpointSet {
    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort;

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort;

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort;
    std::string muxPacketPort;

    MediaGraphQueueParameters queues;
};

::media::Result<void> validateMediaBranchEndpoints(const char* owner,
                                                   const MediaBranchEndpointSet& endpoints);

} // namespace media::ffmpeg::graph

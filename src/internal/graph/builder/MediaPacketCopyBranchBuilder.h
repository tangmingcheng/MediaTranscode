#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaPacketCopyBranchOptions {
    std::string prefix;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    int sourceStreamIndex = invalidMediaStreamIndex;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort;

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort = "codec";
    std::string muxPacketPort = "packet";

    MediaGraphQueueParameters queues;
};

class MediaPacketCopyBranchBuilder final {
public:
    static ::media::Result<void> build(MediaGraph& graph,
                                       const MediaPacketCopyBranchOptions& options);

private:
    MediaPacketCopyBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph

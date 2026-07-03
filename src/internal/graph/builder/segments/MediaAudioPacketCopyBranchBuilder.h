#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioPacketCopyBranchOptions {
    std::string prefix = "audio.copy";
    MediaAudioPipelinePlan plan;
    MediaGraphQueueParameters queues;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "audio";

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort = "codec";
    std::string muxPacketPort = "packet";
};

class MediaAudioPacketCopyBranchBuilder final {
public:
    static ::media::Result<void> build(MediaGraph& graph,
                                       const MediaAudioPacketCopyBranchOptions& options);

private:
    MediaAudioPacketCopyBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph

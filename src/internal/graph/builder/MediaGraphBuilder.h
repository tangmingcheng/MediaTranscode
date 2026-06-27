#pragma once

#include "internal/graph/core/MediaGraph.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaGraphBranchEndpoints {
    MediaNodeId inputNode = MediaNodeId::invalid();
    std::string inputPort;

    MediaNodeId outputNode = MediaNodeId::invalid();
    std::string outputPort;

    bool isValid() const
    {
        return inputNode.isValid() && !inputPort.empty() &&
               outputNode.isValid() && !outputPort.empty();
    }
};

struct MediaGraphVideoBranchOptions {
    std::string prefix = "video";
    bool useHardwareTransfer = true;
};

class MediaGraphBuilder {
public:
    explicit MediaGraphBuilder(MediaGraph& graph);

    MediaGraph& graph();
    const MediaGraph& graph() const;

    MediaNodeId addNode(MediaNodeKind kind,
                        std::string name,
                        std::string diagnosticName = {});

    MediaPortId addInput(MediaNodeId nodeId,
                         std::string name,
                         MediaStreamKind streamKind,
                         MediaEdgeKind edgeKind,
                         MediaPayloadKind payloadKind,
                         bool required = true,
                         bool multiple = false);

    MediaPortId addOutput(MediaNodeId nodeId,
                          std::string name,
                          MediaStreamKind streamKind,
                          MediaEdgeKind edgeKind,
                          MediaPayloadKind payloadKind,
                          bool required = true,
                          bool multiple = false);

    MediaEdgeId connect(MediaNodeId fromNodeId,
                        const std::string& fromPortName,
                        MediaNodeId toNodeId,
                        const std::string& toPortName,
                        std::string edgeName = {},
                        bool required = true);

    MediaGraphBranchEndpoints addVideoTranscodeBranch(const MediaGraphVideoBranchOptions& options = {});

private:
    MediaGraph* m_graph = nullptr;
};

} // namespace media::ffmpeg::graph

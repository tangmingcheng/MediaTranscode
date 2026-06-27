#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/core/MediaNode.h"
#include "internal/graph/core/MediaPort.h"
#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaGraph {
public:
    MediaGraph() = default;

    void clear();

    MediaNodeId addNode(MediaNodeKind kind,
                        std::string name,
                        std::string diagnosticName = {});

    MediaPortId addInputPort(MediaNodeId nodeId,
                             std::string name,
                             MediaStreamKind streamKind,
                             MediaEdgeKind edgeKind,
                             MediaPayloadKind payloadKind,
                             bool required = true,
                             bool multiple = false);

    MediaPortId addOutputPort(MediaNodeId nodeId,
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

    const std::vector<MediaNode>& nodes() const;
    const std::vector<MediaEdge>& edges() const;

    MediaNode* findNode(MediaNodeId id);
    const MediaNode* findNode(MediaNodeId id) const;

    MediaPort* findPort(MediaPortId id);
    const MediaPort* findPort(MediaPortId id) const;

    MediaPort* findInputPort(MediaNodeId nodeId, const std::string& name);
    const MediaPort* findInputPort(MediaNodeId nodeId, const std::string& name) const;

    MediaPort* findOutputPort(MediaNodeId nodeId, const std::string& name);
    const MediaPort* findOutputPort(MediaNodeId nodeId, const std::string& name) const;

    bool empty() const;
    std::size_t nodeCount() const;
    std::size_t edgeCount() const;

private:
    MediaPortId addPort(MediaNodeId nodeId,
                        std::string name,
                        MediaPortDirection direction,
                        MediaStreamKind streamKind,
                        MediaEdgeKind edgeKind,
                        MediaPayloadKind payloadKind,
                        bool required,
                        bool multiple);

    static MediaEdgeKind chooseEdgeKind(const MediaPort& from, const MediaPort& to);
    static MediaStreamKind chooseStreamKind(const MediaPort& from, const MediaPort& to);
    static MediaPayloadKind choosePayloadKind(const MediaPort& from, const MediaPort& to);

private:
    std::vector<MediaNode> m_nodes;
    std::vector<MediaEdge> m_edges;

    uint32_t m_nextNodeId = 1;
    uint32_t m_nextPortId = 1;
    uint32_t m_nextEdgeId = 1;
};

} // namespace media::ffmpeg::graph

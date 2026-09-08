#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/core/MediaNode.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/core/MediaPort.h"
#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

#include <string>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaGraph {
public:
    MediaGraph() = default;

    void clear();

    MediaNodeId addNode(MediaNodeKind kind,
                        std::string name,
                        std::string diagnosticName = {});

    bool setNodeOption(MediaNodeId nodeId, std::string key, std::string value);
    bool setNodeOptions(MediaNodeId nodeId, MediaNodeOptions options);

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

    bool setPortFormatDescriptor(MediaPortId portId, MediaFormatDescriptor descriptor);
    bool setPortTimeDescriptor(MediaPortId portId, MediaTimeDescriptor descriptor);
    bool setPortHardwareDescriptor(MediaPortId portId, MediaHardwareDescriptor descriptor);

    MediaEdgeId connect(MediaNodeId fromNodeId,
                        const std::string& fromPortName,
                        MediaNodeId toNodeId,
                        const std::string& toPortName,
                        std::string edgeName = {},
                        bool required = true);

    MediaEdgeId connect(MediaNodeId fromNodeId,
                        const std::string& fromPortName,
                        MediaNodeId toNodeId,
                        const std::string& toPortName,
                        std::string edgeName,
                        MediaEdgePolicy edgePolicy,
                        bool required = true);

    bool setEdgePolicy(MediaEdgeId edgeId, MediaEdgePolicy policy);

    const std::vector<MediaNode>& nodes() const;
    const std::vector<MediaEdge>& edges() const;

    MediaNode* findNode(MediaNodeId id);
    const MediaNode* findNode(MediaNodeId id) const;

    MediaEdge* findEdge(MediaEdgeId id);
    const MediaEdge* findEdge(MediaEdgeId id) const;

    MediaPort* findPort(MediaPortId id);
    const MediaPort* findPort(MediaPortId id) const;

    MediaPort* findInputPort(MediaNodeId nodeId, const std::string& name);
    const MediaPort* findInputPort(MediaNodeId nodeId, const std::string& name) const;

    MediaPort* findOutputPort(MediaNodeId nodeId, const std::string& name);
    const MediaPort* findOutputPort(MediaNodeId nodeId, const std::string& name) const;

    bool empty() const;
    std::size_t nodeCount() const;
    std::size_t edgeCount() const;
    bool setPayloadCreditPlan(MediaGraphPayloadCreditPlan plan);
    bool setPayloadCreditMode(MediaGraphPayloadCreditMode mode);
    const std::optional<MediaGraphPayloadCreditMode>& payloadCreditMode()
        const noexcept;
    const std::optional<MediaGraphPayloadCreditPlan>& payloadCreditPlan()
        const noexcept;

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
    static MediaFormatDescriptor chooseFormatDescriptor(const MediaPort& from, const MediaPort& to);
    static MediaTimeDescriptor chooseTimeDescriptor(const MediaPort& from, const MediaPort& to);
    static MediaHardwareDescriptor chooseHardwareDescriptor(const MediaPort& from, const MediaPort& to);

private:
    std::vector<MediaNode> m_nodes;
    std::vector<MediaEdge> m_edges;
    std::optional<MediaGraphPayloadCreditPlan> m_payloadCreditPlan;
    std::optional<MediaGraphPayloadCreditMode> m_payloadCreditMode;

    uint32_t m_nextNodeId = 1;
    uint32_t m_nextPortId = 1;
    uint32_t m_nextEdgeId = 1;
};

} // namespace media::ffmpeg::graph

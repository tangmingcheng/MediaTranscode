#include "internal/graph/core/MediaGraph.h"

#include <utility>

namespace media::ffmpeg::graph {

void MediaGraph::clear()
{
    m_nodes.clear();
    m_edges.clear();
    m_payloadCreditPlan.reset();
    m_payloadCreditMode.reset();
    m_nextNodeId = 1;
    m_nextPortId = 1;
    m_nextEdgeId = 1;
}

bool MediaGraph::setPayloadCreditPlan(MediaGraphPayloadCreditPlan plan)
{
    if (m_payloadCreditPlan ||
        (m_payloadCreditMode &&
         *m_payloadCreditMode != MediaGraphPayloadCreditMode::RealtimeRequired) ||
        !plan.isStructurallyValid()) return false;
    m_payloadCreditMode = MediaGraphPayloadCreditMode::RealtimeRequired;
    m_payloadCreditPlan = std::move(plan);
    return true;
}

bool MediaGraph::setPayloadCreditMode(MediaGraphPayloadCreditMode mode)
{
    if (m_payloadCreditMode ||
        (mode == MediaGraphPayloadCreditMode::NonRealtimeNotApplicable &&
         m_payloadCreditPlan)) return false;
    m_payloadCreditMode = mode;
    return true;
}

const std::optional<MediaGraphPayloadCreditMode>&
MediaGraph::payloadCreditMode() const noexcept
{
    return m_payloadCreditMode;
}

const std::optional<MediaGraphPayloadCreditPlan>&
MediaGraph::payloadCreditPlan() const noexcept
{
    return m_payloadCreditPlan;
}

MediaNodeId MediaGraph::addNode(MediaNodeKind kind,
                                std::string name,
                                std::string diagnosticName)
{
    if (kind == MediaNodeKind::Unknown || name.empty()) {
        return MediaNodeId::invalid();
    }

    MediaNode node;
    node.id = MediaNodeId::fromValue(m_nextNodeId++);
    node.kind = kind;
    node.name = std::move(name);
    node.diagnosticName = std::move(diagnosticName);

    m_nodes.push_back(std::move(node));
    return m_nodes.back().id;
}

bool MediaGraph::setNodeOption(MediaNodeId nodeId, std::string key, std::string value)
{
    MediaNode* node = findNode(nodeId);
    if (!node || key.empty()) {
        return false;
    }

    node->options.set(std::move(key), std::move(value));
    return true;
}

bool MediaGraph::setNodeOptions(MediaNodeId nodeId, MediaNodeOptions options)
{
    MediaNode* node = findNode(nodeId);
    if (!node) {
        return false;
    }

    node->options = std::move(options);
    return true;
}

MediaPortId MediaGraph::addInputPort(MediaNodeId nodeId,
                                     std::string name,
                                     MediaStreamKind streamKind,
                                     MediaEdgeKind edgeKind,
                                     MediaPayloadKind payloadKind,
                                     bool required,
                                     bool multiple)
{
    return addPort(
        nodeId,
        std::move(name),
        MediaPortDirection::Input,
        streamKind,
        edgeKind,
        payloadKind,
        required,
        multiple
    );
}

MediaPortId MediaGraph::addOutputPort(MediaNodeId nodeId,
                                      std::string name,
                                      MediaStreamKind streamKind,
                                      MediaEdgeKind edgeKind,
                                      MediaPayloadKind payloadKind,
                                      bool required,
                                      bool multiple)
{
    return addPort(
        nodeId,
        std::move(name),
        MediaPortDirection::Output,
        streamKind,
        edgeKind,
        payloadKind,
        required,
        multiple
    );
}

bool MediaGraph::setPortFormatDescriptor(MediaPortId portId, MediaFormatDescriptor descriptor)
{
    MediaPort* port = findPort(portId);
    if (!port) {
        return false;
    }

    port->format = std::move(descriptor);
    return true;
}

bool MediaGraph::setPortTimeDescriptor(MediaPortId portId, MediaTimeDescriptor descriptor)
{
    MediaPort* port = findPort(portId);
    if (!port) {
        return false;
    }

    port->time = descriptor;
    return true;
}

bool MediaGraph::setPortHardwareDescriptor(MediaPortId portId, MediaHardwareDescriptor descriptor)
{
    MediaPort* port = findPort(portId);
    if (!port) {
        return false;
    }

    port->hardware = std::move(descriptor);
    return true;
}

MediaEdgeId MediaGraph::connect(MediaNodeId fromNodeId,
                                const std::string& fromPortName,
                                MediaNodeId toNodeId,
                                const std::string& toPortName,
                                std::string edgeName,
                                bool required)
{
    return connect(fromNodeId,
                   fromPortName,
                   toNodeId,
                   toPortName,
                   std::move(edgeName),
                   MediaEdgePolicy{},
                   required);
}

MediaEdgeId MediaGraph::connect(MediaNodeId fromNodeId,
                                const std::string& fromPortName,
                                MediaNodeId toNodeId,
                                const std::string& toPortName,
                                std::string edgeName,
                                MediaEdgePolicy edgePolicy,
                                bool required)
{
    const MediaPort* from = findOutputPort(fromNodeId, fromPortName);
    const MediaPort* to = findInputPort(toNodeId, toPortName);

    if (!from || !to || !to->accepts(*from)) {
        return MediaEdgeId::invalid();
    }

    MediaEdge edge;
    edge.id = MediaEdgeId::fromValue(m_nextEdgeId++);
    edge.name = std::move(edgeName);
    edge.from = MediaEdgeEndpoint{ fromNodeId, from->id };
    edge.to = MediaEdgeEndpoint{ toNodeId, to->id };
    edge.streamKind = chooseStreamKind(*from, *to);
    edge.edgeKind = chooseEdgeKind(*from, *to);
    edge.payloadKind = choosePayloadKind(*from, *to);
    edge.format = chooseFormatDescriptor(*from, *to);
    edge.time = chooseTimeDescriptor(*from, *to);
    edge.hardware = chooseHardwareDescriptor(*from, *to);
    edge.policy = std::move(edgePolicy);
    edge.required = required;

    m_edges.push_back(std::move(edge));
    return m_edges.back().id;
}

bool MediaGraph::setEdgePolicy(MediaEdgeId edgeId, MediaEdgePolicy policy)
{
    MediaEdge* edge = findEdge(edgeId);
    if (!edge) {
        return false;
    }

    edge->policy = std::move(policy);
    return true;
}

const std::vector<MediaNode>& MediaGraph::nodes() const
{
    return m_nodes;
}

const std::vector<MediaEdge>& MediaGraph::edges() const
{
    return m_edges;
}

MediaNode* MediaGraph::findNode(MediaNodeId id)
{
    if (!id) {
        return nullptr;
    }

    for (MediaNode& node : m_nodes) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

const MediaNode* MediaGraph::findNode(MediaNodeId id) const
{
    if (!id) {
        return nullptr;
    }

    for (const MediaNode& node : m_nodes) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

MediaEdge* MediaGraph::findEdge(MediaEdgeId id)
{
    if (!id) {
        return nullptr;
    }

    for (MediaEdge& edge : m_edges) {
        if (edge.id == id) {
            return &edge;
        }
    }

    return nullptr;
}

const MediaEdge* MediaGraph::findEdge(MediaEdgeId id) const
{
    if (!id) {
        return nullptr;
    }

    for (const MediaEdge& edge : m_edges) {
        if (edge.id == id) {
            return &edge;
        }
    }

    return nullptr;
}

MediaPort* MediaGraph::findPort(MediaPortId id)
{
    if (!id) {
        return nullptr;
    }

    for (MediaNode& node : m_nodes) {
        for (MediaPort& port : node.inputPorts) {
            if (port.id == id) {
                return &port;
            }
        }

        for (MediaPort& port : node.outputPorts) {
            if (port.id == id) {
                return &port;
            }
        }
    }

    return nullptr;
}

const MediaPort* MediaGraph::findPort(MediaPortId id) const
{
    if (!id) {
        return nullptr;
    }

    for (const MediaNode& node : m_nodes) {
        for (const MediaPort& port : node.inputPorts) {
            if (port.id == id) {
                return &port;
            }
        }

        for (const MediaPort& port : node.outputPorts) {
            if (port.id == id) {
                return &port;
            }
        }
    }

    return nullptr;
}

MediaPort* MediaGraph::findInputPort(MediaNodeId nodeId, const std::string& name)
{
    MediaNode* node = findNode(nodeId);
    if (!node) {
        return nullptr;
    }

    for (MediaPort& port : node->inputPorts) {
        if (port.name == name) {
            return &port;
        }
    }

    return nullptr;
}

const MediaPort* MediaGraph::findInputPort(MediaNodeId nodeId, const std::string& name) const
{
    const MediaNode* node = findNode(nodeId);
    if (!node) {
        return nullptr;
    }

    for (const MediaPort& port : node->inputPorts) {
        if (port.name == name) {
            return &port;
        }
    }

    return nullptr;
}

MediaPort* MediaGraph::findOutputPort(MediaNodeId nodeId, const std::string& name)
{
    MediaNode* node = findNode(nodeId);
    if (!node) {
        return nullptr;
    }

    for (MediaPort& port : node->outputPorts) {
        if (port.name == name) {
            return &port;
        }
    }

    return nullptr;
}

const MediaPort* MediaGraph::findOutputPort(MediaNodeId nodeId, const std::string& name) const
{
    const MediaNode* node = findNode(nodeId);
    if (!node) {
        return nullptr;
    }

    for (const MediaPort& port : node->outputPorts) {
        if (port.name == name) {
            return &port;
        }
    }

    return nullptr;
}

bool MediaGraph::empty() const
{
    return m_nodes.empty();
}

std::size_t MediaGraph::nodeCount() const
{
    return m_nodes.size();
}

std::size_t MediaGraph::edgeCount() const
{
    return m_edges.size();
}

MediaPortId MediaGraph::addPort(MediaNodeId nodeId,
                                std::string name,
                                MediaPortDirection direction,
                                MediaStreamKind streamKind,
                                MediaEdgeKind edgeKind,
                                MediaPayloadKind payloadKind,
                                bool required,
                                bool multiple)
{
    if (!nodeId || name.empty() || direction == MediaPortDirection::Unknown) {
        return MediaPortId::invalid();
    }

    MediaNode* node = findNode(nodeId);
    if (!node) {
        return MediaPortId::invalid();
    }

    MediaPort port;
    port.id = MediaPortId::fromValue(m_nextPortId++);
    port.nodeId = nodeId;
    port.name = std::move(name);
    port.direction = direction;
    port.streamKind = streamKind;
    port.edgeKind = edgeKind;
    port.payloadKind = payloadKind;
    port.format.streamKind = streamKind;
    port.required = required;
    port.multiple = multiple;

    if (direction == MediaPortDirection::Input) {
        node->inputPorts.push_back(std::move(port));
        return node->inputPorts.back().id;
    }

    node->outputPorts.push_back(std::move(port));
    return node->outputPorts.back().id;
}

MediaEdgeKind MediaGraph::chooseEdgeKind(const MediaPort& from, const MediaPort& to)
{
    if (from.edgeKind != MediaEdgeKind::Unknown) {
        return from.edgeKind;
    }

    return to.edgeKind;
}

MediaStreamKind MediaGraph::chooseStreamKind(const MediaPort& from, const MediaPort& to)
{
    if (from.streamKind != MediaStreamKind::Any &&
        from.streamKind != MediaStreamKind::Unknown) {
        return from.streamKind;
    }

    return to.streamKind;
}

MediaPayloadKind MediaGraph::choosePayloadKind(const MediaPort& from, const MediaPort& to)
{
    if (from.payloadKind != MediaPayloadKind::Unknown) {
        return from.payloadKind;
    }

    return to.payloadKind;
}

MediaFormatDescriptor MediaGraph::chooseFormatDescriptor(const MediaPort& from, const MediaPort& to)
{
    if (from.hasFormatDescriptor()) {
        return from.format;
    }

    return to.format;
}

MediaTimeDescriptor MediaGraph::chooseTimeDescriptor(const MediaPort& from, const MediaPort& to)
{
    if (from.hasTimeDescriptor()) {
        return from.time;
    }

    return to.time;
}

MediaHardwareDescriptor MediaGraph::chooseHardwareDescriptor(const MediaPort& from, const MediaPort& to)
{
    if (from.hasHardwareDescriptor()) {
        return from.hardware;
    }

    return to.hardware;
}

} // namespace media::ffmpeg::graph

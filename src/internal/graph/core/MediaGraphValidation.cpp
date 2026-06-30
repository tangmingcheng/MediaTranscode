#include "internal/graph/core/MediaGraphValidation.h"

#include "internal/graph/core/MediaGraphTopology.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {

bool MediaGraphValidationReport::ok() const
{
    for (const auto& issue : issues) {
        if (issue.severity == MediaGraphValidationSeverity::Error) {
            return false;
        }
    }

    return true;
}

bool MediaGraphValidationReport::hasErrors() const
{
    return !ok();
}

std::size_t MediaGraphValidationReport::errorCount() const
{
    std::size_t count = 0;
    for (const auto& issue : issues) {
        if (issue.severity == MediaGraphValidationSeverity::Error) {
            ++count;
        }
    }

    return count;
}

std::size_t MediaGraphValidationReport::warningCount() const
{
    std::size_t count = 0;
    for (const auto& issue : issues) {
        if (issue.severity == MediaGraphValidationSeverity::Warning) {
            ++count;
        }
    }

    return count;
}

namespace {

void addIssue(MediaGraphValidationReport& report,
              MediaGraphValidationSeverity severity,
              MediaGraphErrorCode code,
              std::string message,
              MediaNodeId node = MediaNodeId::invalid(),
              MediaPortId port = MediaPortId::invalid(),
              MediaEdgeId edge = MediaEdgeId::invalid())
{
    report.issues.push_back({ severity, code, std::move(message), node, port, edge });
}

void validateAcyclic(MediaGraphValidationReport& report, const MediaGraph& graph)
{
    auto topology = MediaGraphTopology::build(graph);
    if (!topology) {
        addIssue(report,
                 MediaGraphValidationSeverity::Error,
                 MediaGraphErrorCode::CycleDetected,
                 "Cycle detected in media DAG");
    }
}

void validatePortDescriptors(MediaGraphValidationReport& report, const MediaNode& node, const MediaPort& port)
{
    if (port.format.streamKind != MediaStreamKind::Unknown &&
        port.format.streamKind != MediaStreamKind::Any &&
        port.streamKind != MediaStreamKind::Any &&
        port.streamKind != MediaStreamKind::Unknown &&
        port.format.streamKind != port.streamKind) {
        addIssue(report,
                 MediaGraphValidationSeverity::Warning,
                 MediaGraphErrorCode::InvalidDescriptor,
                 "Port format descriptor stream kind differs from port stream kind",
                 node.id,
                 port.id);
    }

    if (port.time.timeBase.isValid() == false || port.time.frameRate.isValid() == false) {
        addIssue(report,
                 MediaGraphValidationSeverity::Error,
                 MediaGraphErrorCode::InvalidDescriptor,
                 "Port time descriptor contains invalid rational",
                 node.id,
                 port.id);
    }
}

void validateEdgePolicy(MediaGraphValidationReport& report, const MediaEdge& edge)
{
    const auto& queuePolicy = edge.policy.queuePolicy;

    if (queuePolicy.mode == MediaQueueMode::Unknown) {
        addIssue(report,
                 MediaGraphValidationSeverity::Error,
                 MediaGraphErrorCode::InvalidPolicy,
                 "Edge queue policy mode is unknown",
                 MediaNodeId::invalid(),
                 MediaPortId::invalid(),
                 edge.id);
    }

    if (queuePolicy.bounded && queuePolicy.mode != MediaQueueMode::Direct && queuePolicy.capacity == 0) {
        addIssue(report,
                 MediaGraphValidationSeverity::Error,
                 MediaGraphErrorCode::InvalidPolicy,
                 "Bounded edge queue policy requires capacity > 0",
                 MediaNodeId::invalid(),
                 MediaPortId::invalid(),
                 edge.id);
    }

    if (edge.time.timeBase.isValid() == false || edge.time.frameRate.isValid() == false) {
        addIssue(report,
                 MediaGraphValidationSeverity::Error,
                 MediaGraphErrorCode::InvalidDescriptor,
                 "Edge time descriptor contains invalid rational",
                 MediaNodeId::invalid(),
                 MediaPortId::invalid(),
                 edge.id);
    }
}

} // namespace

MediaGraphValidationReport MediaGraphValidation::validate(const MediaGraph& graph)
{
    MediaGraphValidationReport report;

    const auto& nodes = graph.nodes();
    const auto& edges = graph.edges();

    std::unordered_map<uint32_t, bool> nodeSeen;
    std::unordered_set<uint32_t> portSeen;

    for (const auto& node : nodes) {
        if (!node.isValid()) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::InvalidNode,
                     "Invalid node detected",
                     node.id);
            continue;
        }

        if (nodeSeen[node.id.value]) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::DuplicateId,
                     "Duplicate node id detected",
                     node.id);
        }
        nodeSeen[node.id.value] = true;

        std::unordered_map<std::string, bool> inputNames;
        for (const auto& port : node.inputPorts) {
            if (!port.id || port.nodeId != node.id || !port.isInput() || port.name.empty()) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::InvalidPort,
                         "Invalid input port detected",
                         node.id,
                         port.id);
            }

            if (!portSeen.insert(port.id.value).second) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::DuplicateId,
                         "Duplicate port id detected",
                         node.id,
                         port.id);
            }

            if (inputNames[port.name]) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::DuplicatePortName,
                         "Duplicate input port name detected",
                         node.id,
                         port.id);
            }
            inputNames[port.name] = true;
            validatePortDescriptors(report, node, port);
        }

        std::unordered_map<std::string, bool> outputNames;
        for (const auto& port : node.outputPorts) {
            if (!port.id || port.nodeId != node.id || !port.isOutput() || port.name.empty()) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::InvalidPort,
                         "Invalid output port detected",
                         node.id,
                         port.id);
            }

            if (!portSeen.insert(port.id.value).second) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::DuplicateId,
                         "Duplicate port id detected",
                         node.id,
                         port.id);
            }

            if (outputNames[port.name]) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::DuplicatePortName,
                         "Duplicate output port name detected",
                         node.id,
                         port.id);
            }
            outputNames[port.name] = true;
            validatePortDescriptors(report, node, port);
        }
    }

    std::unordered_map<uint32_t, int> inputPortUsage;
    std::unordered_map<uint32_t, int> outputPortUsage;
    std::unordered_map<uint32_t, bool> edgeSeen;

    for (const auto& edge : edges) {
        if (!edge.isValid()) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::InvalidEdge,
                     "Invalid edge detected",
                     MediaNodeId::invalid(),
                     MediaPortId::invalid(),
                     edge.id);
            continue;
        }

        if (edgeSeen[edge.id.value]) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::DuplicateId,
                     "Duplicate edge id detected",
                     MediaNodeId::invalid(),
                     MediaPortId::invalid(),
                     edge.id);
        }
        edgeSeen[edge.id.value] = true;

        const MediaNode* fromNode = graph.findNode(edge.from.nodeId);
        const MediaNode* toNode = graph.findNode(edge.to.nodeId);

        if (!fromNode || !toNode) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::MissingNode,
                     "Edge references missing node",
                     MediaNodeId::invalid(),
                     MediaPortId::invalid(),
                     edge.id);
            continue;
        }

        const MediaPort* fromPort = graph.findPort(edge.from.portId);
        const MediaPort* toPort = graph.findPort(edge.to.portId);

        if (!fromPort || !toPort) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::MissingPort,
                     "Edge references missing port",
                     MediaNodeId::invalid(),
                     MediaPortId::invalid(),
                     edge.id);
            continue;
        }

        if (!fromPort->isOutput() || !toPort->isInput()) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::PortDirectionMismatch,
                     "Edge direction invalid",
                     edge.from.nodeId,
                     edge.from.portId,
                     edge.id);
        }

        if (fromPort->nodeId != edge.from.nodeId || toPort->nodeId != edge.to.nodeId) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::PortDirectionMismatch,
                     "Edge port-node mismatch",
                     MediaNodeId::invalid(),
                     MediaPortId::invalid(),
                     edge.id);
        }

        if (!toPort->accepts(*fromPort)) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::PortTypeMismatch,
                     "Port type mismatch",
                     edge.to.nodeId,
                     edge.to.portId,
                     edge.id);
        }

        ++outputPortUsage[fromPort->id.value];
        ++inputPortUsage[toPort->id.value];

        if (!toPort->multiple && inputPortUsage[toPort->id.value] > 1) {
            addIssue(report,
                     MediaGraphValidationSeverity::Error,
                     MediaGraphErrorCode::InputMultiplicityViolation,
                     "Input port used multiple times but multiple=false",
                     edge.to.nodeId,
                     edge.to.portId,
                     edge.id);
        }

        validateEdgePolicy(report, edge);
    }

    for (const auto& node : nodes) {
        for (const auto& port : node.inputPorts) {
            if (port.required && inputPortUsage[port.id.value] == 0) {
                addIssue(report,
                         MediaGraphValidationSeverity::Error,
                         MediaGraphErrorCode::RequiredInputMissing,
                         "Required input port not connected",
                         node.id,
                         port.id);
            }
        }

        for (const auto& port : node.outputPorts) {
            if (port.required && outputPortUsage[port.id.value] == 0) {
                addIssue(report,
                         MediaGraphValidationSeverity::Warning,
                         MediaGraphErrorCode::RequiredOutputUnused,
                         "Required output port has no consumers",
                         node.id,
                         port.id);
            }
        }
    }

    validateAcyclic(report, graph);

    return report;
}

} // namespace media::ffmpeg::graph

#include "internal/graph/core/MediaGraphValidation.h"

#include <unordered_map>

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
              std::string message,
              MediaNodeId node = MediaNodeId::invalid(),
              MediaPortId port = MediaPortId::invalid(),
              MediaEdgeId edge = MediaEdgeId::invalid())
{
    report.issues.push_back({ severity, std::move(message), node, port, edge });
}

} // namespace

MediaGraphValidationReport MediaGraphValidation::validate(const MediaGraph& graph)
{
    MediaGraphValidationReport report;

    const auto& nodes = graph.nodes();
    const auto& edges = graph.edges();

    std::unordered_map<uint32_t, bool> nodeSeen;
    for (const auto& node : nodes) {
        if (!node.isValid()) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Invalid node detected", node.id);
            continue;
        }

        if (nodeSeen[node.id.value]) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Duplicate node id detected", node.id);
        }
        nodeSeen[node.id.value] = true;

        std::unordered_map<std::string, bool> inputNames;
        for (const auto& port : node.inputPorts) {
            if (!port.id || port.nodeId != node.id || !port.isInput() || port.name.empty()) {
                addIssue(report, MediaGraphValidationSeverity::Error,
                         "Invalid input port detected", node.id, port.id);
            }

            if (inputNames[port.name]) {
                addIssue(report, MediaGraphValidationSeverity::Error,
                         "Duplicate input port name detected", node.id, port.id);
            }
            inputNames[port.name] = true;
        }

        std::unordered_map<std::string, bool> outputNames;
        for (const auto& port : node.outputPorts) {
            if (!port.id || port.nodeId != node.id || !port.isOutput() || port.name.empty()) {
                addIssue(report, MediaGraphValidationSeverity::Error,
                         "Invalid output port detected", node.id, port.id);
            }

            if (outputNames[port.name]) {
                addIssue(report, MediaGraphValidationSeverity::Error,
                         "Duplicate output port name detected", node.id, port.id);
            }
            outputNames[port.name] = true;
        }
    }

    std::unordered_map<uint32_t, int> inputPortUsage;
    std::unordered_map<uint32_t, int> outputPortUsage;
    std::unordered_map<uint32_t, bool> edgeSeen;

    for (const auto& edge : edges) {
        if (!edge.isValid()) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Invalid edge detected",
                     MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
            continue;
        }

        if (edgeSeen[edge.id.value]) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Duplicate edge id detected",
                     MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
        }
        edgeSeen[edge.id.value] = true;

        const MediaNode* fromNode = graph.findNode(edge.from.nodeId);
        const MediaNode* toNode = graph.findNode(edge.to.nodeId);

        if (!fromNode || !toNode) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge references missing node",
                     MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
            continue;
        }

        const MediaPort* fromPort = graph.findPort(edge.from.portId);
        const MediaPort* toPort = graph.findPort(edge.to.portId);

        if (!fromPort || !toPort) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge references missing port",
                     MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
            continue;
        }

        if (!fromPort->isOutput() || !toPort->isInput()) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge direction invalid",
                     edge.from.nodeId, edge.from.portId, edge.id);
        }

        if (fromPort->nodeId != edge.from.nodeId || toPort->nodeId != edge.to.nodeId) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge port-node mismatch",
                     MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
        }

        if (!toPort->accepts(*fromPort)) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Port type mismatch",
                     edge.to.nodeId, edge.to.portId, edge.id);
        }

        ++outputPortUsage[fromPort->id.value];
        ++inputPortUsage[toPort->id.value];

        if (!toPort->multiple && inputPortUsage[toPort->id.value] > 1) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Input port used multiple times but multiple=false",
                     edge.to.nodeId, edge.to.portId, edge.id);
        }
    }

    for (const auto& node : nodes) {
        for (const auto& port : node.inputPorts) {
            if (port.required && inputPortUsage[port.id.value] == 0) {
                addIssue(report, MediaGraphValidationSeverity::Error,
                         "Required input port not connected",
                         node.id, port.id);
            }
        }

        for (const auto& port : node.outputPorts) {
            if (port.required && outputPortUsage[port.id.value] == 0) {
                addIssue(report, MediaGraphValidationSeverity::Warning,
                         "Required output port has no consumers",
                         node.id, port.id);
            }
        }
    }

    return report;
}

} // namespace media::ffmpeg::graph

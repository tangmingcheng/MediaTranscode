#include "internal/graph/core/MediaGraphValidation.h"

#include <unordered_map>
#include <sstream>

namespace media::ffmpeg::graph {

bool MediaGraphValidationReport::ok() const
{
    for (const auto& i : issues) {
        if (i.severity == MediaGraphValidationSeverity::Error) {
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
    for (const auto& i : issues) {
        if (i.severity == MediaGraphValidationSeverity::Error) {
            ++count;
        }
    }
    return count;
}

std::size_t MediaGraphValidationReport::warningCount() const
{
    std::size_t count = 0;
    for (const auto& i : issues) {
        if (i.severity == MediaGraphValidationSeverity::Warning) {
            ++count;
        }
    }
    return count;
}

static void addIssue(MediaGraphValidationReport& report,
                     MediaGraphValidationSeverity sev,
                     const std::string& msg,
                     MediaNodeId node = MediaNodeId::invalid(),
                     MediaPortId port = MediaPortId::invalid(),
                     MediaEdgeId edge = MediaEdgeId::invalid())
{
    report.issues.push_back({ sev, msg, node, port, edge });
}

MediaGraphValidationReport MediaGraphValidation::validate(const MediaGraph& graph)
{
    MediaGraphValidationReport report;

    const auto& nodes = graph.nodes();
    const auto& edges = graph.edges();

    // 1. Validate nodes
    std::unordered_map<uint32_t, bool> nodeSeen;
    for (const auto& node : nodes) {
        if (!node.isValid()) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Invalid node detected");
            continue;
        }

        if (nodeSeen[node.id.value]) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Duplicate node id detected", node.id);
        }
        nodeSeen[node.id.value] = true;

        if (node.name.empty()) {
            addIssue(report, MediaGraphValidationSeverity::Warning,
                     "Node has empty name", node.id);
        }
    }

    // 2. Validate edges
    std::unordered_map<uint32_t, int> inputPortUsage;

    for (const auto& edge : edges) {
        if (!edge.isValid()) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Invalid edge detected", MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
            continue;
        }

        const MediaNode* fromNode = graph.findNode(edge.from.nodeId);
        const MediaNode* toNode = graph.findNode(edge.to.nodeId);

        if (!fromNode || !toNode) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge references missing node", MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
            continue;
        }

        const MediaPort* fromPort = graph.findPort(edge.from.portId);
        const MediaPort* toPort = graph.findPort(edge.to.portId);

        if (!fromPort || !toPort) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge references missing port", MediaNodeId::invalid(), MediaPortId::invalid(), edge.id);
            continue;
        }

        // direction sanity
        if (!fromPort->isOutput() || !toPort->isInput()) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge direction invalid", edge.from.nodeId, edge.from.portId, edge.id);
        }

        // port-node consistency
        if (fromPort->nodeId != edge.from.nodeId || toPort->nodeId != edge.to.nodeId) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Edge port-node mismatch", edge.id);
        }

        // accepts check
        if (!toPort->accepts(*fromPort)) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Port type mismatch", edge.id);
        }

        // usage tracking
        inputPortUsage[toPort->id.value]++;

        if (!toPort->multiple && inputPortUsage[toPort->id.value] > 1) {
            addIssue(report, MediaGraphValidationSeverity::Error,
                     "Input port used multiple times but multiple=false",
                     edge.to.nodeId, edge.to.portId);
        }
    }

    // 3. Validate required ports
    for (const auto& node : nodes) {
        for (const auto& port : node.inputPorts) {
            if (port.required && inputPortUsage[port.id.value] == 0) {
                addIssue(report, MediaGraphValidationSeverity::Error,
                         "Required input port not connected",
                         node.id, port.id);
            }
        }
    }

    return report;
}

} // namespace media::ffmpeg::graph

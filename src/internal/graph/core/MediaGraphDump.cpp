#include "internal/graph/core/MediaGraphDump.h"

#include <sstream>

namespace media::ffmpeg::graph {

static const char* toString(MediaNodeKind k)
{
    switch (k) {
    case MediaNodeKind::FileInput: return "FileInput";
    case MediaNodeKind::RealtimeInput: return "RealtimeInput";
    case MediaNodeKind::Demux: return "Demux";
    case MediaNodeKind::StreamSplit: return "StreamSplit";
    case MediaNodeKind::VideoDecode: return "VideoDecode";
    case MediaNodeKind::AudioDecode: return "AudioDecode";
    case MediaNodeKind::VideoEncode: return "VideoEncode";
    case MediaNodeKind::AudioEncode: return "AudioEncode";
    case MediaNodeKind::FileOutput: return "FileOutput";
    case MediaNodeKind::RtpOutput: return "RtpOutput";
    case MediaNodeKind::FileMux: return "FileMux";
    case MediaNodeKind::RtpMux: return "RtpMux";
    case MediaNodeKind::VideoFilter: return "VideoFilter";
    case MediaNodeKind::FrameRoute: return "FrameRoute";
    default: return "Unknown";
    }
}

std::string MediaGraphDump::toText(const MediaGraph& graph)
{
    std::ostringstream oss;

    oss << "MediaGraph Dump\n";
    oss << "Nodes: " << graph.nodeCount() << " Edges: " << graph.edgeCount() << "\n\n";

    for (const auto& node : graph.nodes()) {
        oss << "[Node " << node.id.value << "] " << toString(node.kind)
            << " (" << node.name << ")\n";

        for (const auto& p : node.outputPorts) {
            oss << "  out: " << p.name << " ->\n";
        }

        for (const auto& p : node.inputPorts) {
            oss << "  in: " << p.name << "\n";
        }

        oss << "\n";
    }

    for (const auto& e : graph.edges()) {
        oss << "Edge " << e.id.value << ": "
            << e.from.nodeId.value << ":" << e.from.portId.value
            << " -> "
            << e.to.nodeId.value << ":" << e.to.portId.value
            << "\n";
    }

    return oss.str();
}

std::string MediaGraphDump::toGraphvizDot(const MediaGraph& graph,
                                          const std::string& graphName)
{
    std::ostringstream oss;

    oss << "digraph " << graphName << " {\n";
    oss << "rankdir=LR;\n";
    oss << "node [shape=box];\n\n";

    for (const auto& node : graph.nodes()) {
        oss << "n" << node.id.value
            << " [label=\"" << toString(node.kind)
            << "\\n" << node.name << "\"];
\n";
    }

    for (const auto& e : graph.edges()) {
        oss << "n" << e.from.nodeId.value
            << " -> "
            << "n" << e.to.nodeId.value
            << ";\n";
    }

    oss << "}\n";

    return oss.str();
}

} // namespace media::ffmpeg::graph

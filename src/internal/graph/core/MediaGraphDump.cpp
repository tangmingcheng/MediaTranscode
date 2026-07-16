#include "internal/graph/core/MediaGraphDump.h"

#include <sstream>

namespace media::ffmpeg::graph {

namespace {

const char* toString(MediaNodeKind kind)
{
    switch (kind) {
    case MediaNodeKind::FileInput: return "FileInput";
    case MediaNodeKind::RealtimeInput: return "RealtimeInput";
    case MediaNodeKind::RawRtpInput: return "RawRtpInput";
    case MediaNodeKind::Demux: return "Demux";
    case MediaNodeKind::MpegTsDemux: return "MpegTsDemux";
    case MediaNodeKind::StreamSplit: return "StreamSplit";
    case MediaNodeKind::PacketFanout: return "PacketFanout";
    case MediaNodeKind::FrameRoute: return "FrameRoute";
    case MediaNodeKind::VideoDecode: return "VideoDecode";
    case MediaNodeKind::VideoTimestamp: return "VideoTimestamp";
    case MediaNodeKind::HardwareTransfer: return "HardwareTransfer";
    case MediaNodeKind::VideoFrameRate: return "VideoFrameRate";
    case MediaNodeKind::VideoFilter: return "VideoFilter";
    case MediaNodeKind::VideoEncode: return "VideoEncode";
    case MediaNodeKind::AudioCodecResolver: return "AudioCodecResolver";
    case MediaNodeKind::AudioDecode: return "AudioDecode";
    case MediaNodeKind::AudioResample: return "AudioResample";
    case MediaNodeKind::AudioEncode: return "AudioEncode";
    case MediaNodeKind::PacketSourceConfig: return "PacketSourceConfig";
    case MediaNodeKind::PacketNormalize: return "PacketNormalize";
    case MediaNodeKind::AvPacketStartBarrier: return "AvPacketStartBarrier";
    case MediaNodeKind::PacketStartGate: return "PacketStartGate";
    case MediaNodeKind::RtpClockGroup: return "RtpClockGroup";
    case MediaNodeKind::RtpPacketClockBinder: return "RtpPacketClockBinder";
    case MediaNodeKind::RtpClockSnapshotFanout: return "RtpClockSnapshotFanout";
    case MediaNodeKind::AvStartupCoordinator: return "AvStartupCoordinator";
    case MediaNodeKind::AvOutputScheduler: return "AvOutputScheduler";
    case MediaNodeKind::PlaybackEpochBinder: return "PlaybackEpochBinder";
    case MediaNodeKind::CanonicalInput: return "CanonicalInput";
    case MediaNodeKind::AvBoundReleaseExtractor: return "AvBoundReleaseExtractor";
    case MediaNodeKind::PacketMerge: return "PacketMerge";
    case MediaNodeKind::FileMux: return "FileMux";
    case MediaNodeKind::RtpMux: return "RtpMux";
    case MediaNodeKind::FileOutput: return "FileOutput";
    case MediaNodeKind::RtpOutput: return "RtpOutput";
    case MediaNodeKind::SdpWriter: return "SdpWriter";
    case MediaNodeKind::EofBarrier: return "EofBarrier";
    case MediaNodeKind::Flush: return "Flush";
    case MediaNodeKind::Finalize: return "Finalize";
    case MediaNodeKind::ControlSignal: return "ControlSignal";
    case MediaNodeKind::MetadataProbe: return "MetadataProbe";
    case MediaNodeKind::DebugDump: return "DebugDump";
    case MediaNodeKind::TraceProbe: return "TraceProbe";
    case MediaNodeKind::CodecResolver: return "CodecResolver";
    case MediaNodeKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* toString(MediaStreamKind kind)
{
    switch (kind) {
    case MediaStreamKind::Video: return "Video";
    case MediaStreamKind::Audio: return "Audio";
    case MediaStreamKind::Subtitle: return "Subtitle";
    case MediaStreamKind::Data: return "Data";
    case MediaStreamKind::Attachment: return "Attachment";
    case MediaStreamKind::Control: return "Control";
    case MediaStreamKind::Metadata: return "Metadata";
    case MediaStreamKind::Any: return "Any";
    case MediaStreamKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* toString(MediaEdgeKind kind)
{
    switch (kind) {
    case MediaEdgeKind::InputPacket: return "InputPacket";
    case MediaEdgeKind::RawFrame: return "RawFrame";
    case MediaEdgeKind::HardwareFrame: return "HardwareFrame";
    case MediaEdgeKind::SoftwareFrame: return "SoftwareFrame";
    case MediaEdgeKind::DecoderConfig: return "DecoderConfig";
    case MediaEdgeKind::EncoderConfig: return "EncoderConfig";
    case MediaEdgeKind::StreamConfig: return "StreamConfig";
    case MediaEdgeKind::EncodedPacket: return "EncodedPacket";
    case MediaEdgeKind::CopiedPacket: return "CopiedPacket";
    case MediaEdgeKind::MuxedPacket: return "MuxedPacket";
    case MediaEdgeKind::Metadata: return "Metadata";
    case MediaEdgeKind::Control: return "Control";
    case MediaEdgeKind::Event: return "Event";
    case MediaEdgeKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* toString(MediaPayloadKind kind)
{
    switch (kind) {
    case MediaPayloadKind::FormatContext: return "FormatContext";
    case MediaPayloadKind::StreamDescriptor: return "StreamDescriptor";
    case MediaPayloadKind::CodecContext: return "CodecContext";
    case MediaPayloadKind::CodecParameters: return "CodecParameters";
    case MediaPayloadKind::Packet: return "Packet";
    case MediaPayloadKind::Frame: return "Frame";
    case MediaPayloadKind::TimeDescriptor: return "TimeDescriptor";
    case MediaPayloadKind::HardwareDescriptor: return "HardwareDescriptor";
    case MediaPayloadKind::AudioLayoutDescriptor: return "AudioLayoutDescriptor";
    case MediaPayloadKind::VideoFormatDescriptor: return "VideoFormatDescriptor";
    case MediaPayloadKind::ControlSignal: return "ControlSignal";
    case MediaPayloadKind::GraphEvent: return "GraphEvent";
    case MediaPayloadKind::DiagnosticRecord: return "DiagnosticRecord";
    case MediaPayloadKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* toString(MediaQueueMode mode)
{
    switch (mode) {
    case MediaQueueMode::Direct: return "Direct";
    case MediaQueueMode::Blocking: return "Blocking";
    case MediaQueueMode::SpscRing: return "SpscRing";
    case MediaQueueMode::MpscRing: return "MpscRing";
    case MediaQueueMode::Unknown:
    default:
        return "Unknown";
    }
}

const char* toString(MediaQueueOverflowPolicy policy)
{
    switch (policy) {
    case MediaQueueOverflowPolicy::BlockProducer: return "BlockProducer";
    case MediaQueueOverflowPolicy::DropNewest: return "DropNewest";
    case MediaQueueOverflowPolicy::DropOldest: return "DropOldest";
    case MediaQueueOverflowPolicy::DropNonKeyFrame: return "DropNonKeyFrame";
    case MediaQueueOverflowPolicy::Abort: return "Abort";
    default:
        return "Unknown";
    }
}

std::string dotEscape(const std::string& text)
{
    std::string output;
    output.reserve(text.size());

    for (const char ch : text) {
        switch (ch) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        default:
            output += ch;
            break;
        }
    }

    return output;
}

std::string nodeLabel(const MediaNode& node)
{
    std::string label = std::string(toString(node.kind)) + "\n" + node.name;
    if (!node.diagnosticName.empty()) {
        label += "\n" + node.diagnosticName;
    }
    return dotEscape(label);
}

std::string edgeLabel(const MediaGraph& graph, const MediaEdge& edge)
{
    const MediaPort* fromPort = graph.findPort(edge.from.portId);
    const MediaPort* toPort = graph.findPort(edge.to.portId);

    std::string label;
    if (fromPort) {
        label += fromPort->name;
    }
    label += " -> ";
    if (toPort) {
        label += toPort->name;
    }
    label += "\n";
    label += toString(edge.streamKind);
    label += " / ";
    label += toString(edge.edgeKind);
    label += " / ";
    label += toString(edge.payloadKind);
    label += "\nqueue=";
    label += toString(edge.policy.queuePolicy.mode);
    label += " cap=";
    label += std::to_string(edge.policy.queuePolicy.capacity);

    return dotEscape(label);
}

} // namespace

std::string MediaGraphDump::toText(const MediaGraph& graph)
{
    std::ostringstream oss;

    oss << "MediaGraph Dump\n";
    oss << "Nodes: " << graph.nodeCount() << " Edges: " << graph.edgeCount() << "\n\n";

    for (const auto& node : graph.nodes()) {
        oss << "[Node " << node.id.value << "] " << toString(node.kind)
            << " (" << node.name << ")\n";

        for (const auto& port : node.inputPorts) {
            oss << "  in : " << port.name
                << " [" << toString(port.streamKind)
                << ", " << toString(port.edgeKind)
                << ", " << toString(port.payloadKind)
                << "]\n";
        }

        for (const auto& port : node.outputPorts) {
            oss << "  out: " << port.name
                << " [" << toString(port.streamKind)
                << ", " << toString(port.edgeKind)
                << ", " << toString(port.payloadKind)
                << "]\n";
        }

        oss << "\n";
    }

    oss << "Edges:\n";
    for (const auto& edge : graph.edges()) {
        const MediaPort* fromPort = graph.findPort(edge.from.portId);
        const MediaPort* toPort = graph.findPort(edge.to.portId);

        oss << "  [Edge " << edge.id.value << "] "
            << edge.from.nodeId.value << ":"
            << (fromPort ? fromPort->name : "<missing>")
            << " -> "
            << edge.to.nodeId.value << ":"
            << (toPort ? toPort->name : "<missing>")
            << " [stream=" << toString(edge.streamKind)
            << ", edge=" << toString(edge.edgeKind)
            << ", payload=" << toString(edge.payloadKind)
            << ", queue=" << toString(edge.policy.queuePolicy.mode)
            << ", capacity=" << edge.policy.queuePolicy.capacity
            << ", overflow=" << toString(edge.policy.queuePolicy.overflowPolicy)
            << "]\n";
    }

    return oss.str();
}

std::string MediaGraphDump::toGraphvizDot(const MediaGraph& graph,
                                          const std::string& graphName)
{
    std::ostringstream oss;

    oss << "digraph " << graphName << " {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box, fontname=\"Consolas\"];\n";
    oss << "  edge [fontname=\"Consolas\"];\n\n";

    for (const auto& node : graph.nodes()) {
        oss << "  n" << node.id.value << " [label=\"" << nodeLabel(node) << "\"];\n";
    }

    oss << "\n";

    for (const auto& edge : graph.edges()) {
        oss << "  n" << edge.from.nodeId.value
            << " -> n" << edge.to.nodeId.value
            << " [label=\"" << edgeLabel(graph, edge) << "\"];\n";
    }

    oss << "}\n";
    return oss.str();
}

} // namespace media::ffmpeg::graph

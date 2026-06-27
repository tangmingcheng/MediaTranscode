#include "internal/graph/builder/MediaGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaGraphBuilder::MediaGraphBuilder(MediaGraph& graph)
    : m_graph(&graph)
{
}

MediaGraph& MediaGraphBuilder::graph()
{
    return *m_graph;
}

const MediaGraph& MediaGraphBuilder::graph() const
{
    return *m_graph;
}

MediaNodeId MediaGraphBuilder::addNode(MediaNodeKind kind,
                                      std::string name,
                                      std::string diagnosticName)
{
    return m_graph->addNode(kind, std::move(name), std::move(diagnosticName));
}

MediaPortId MediaGraphBuilder::addInput(MediaNodeId nodeId,
                                        std::string name,
                                        MediaStreamKind streamKind,
                                        MediaEdgeKind edgeKind,
                                        MediaPayloadKind payloadKind,
                                        bool required,
                                        bool multiple)
{
    return m_graph->addInputPort(nodeId,
                                 std::move(name),
                                 streamKind,
                                 edgeKind,
                                 payloadKind,
                                 required,
                                 multiple);
}

MediaPortId MediaGraphBuilder::addOutput(MediaNodeId nodeId,
                                         std::string name,
                                         MediaStreamKind streamKind,
                                         MediaEdgeKind edgeKind,
                                         MediaPayloadKind payloadKind,
                                         bool required,
                                         bool multiple)
{
    return m_graph->addOutputPort(nodeId,
                                  std::move(name),
                                  streamKind,
                                  edgeKind,
                                  payloadKind,
                                  required,
                                  multiple);
}

MediaEdgeId MediaGraphBuilder::connect(MediaNodeId fromNodeId,
                                      const std::string& fromPortName,
                                      MediaNodeId toNodeId,
                                      const std::string& toPortName,
                                      std::string edgeName,
                                      bool required)
{
    return m_graph->connect(fromNodeId,
                           fromPortName,
                           toNodeId,
                           toPortName,
                           std::move(edgeName),
                           required);
}

MediaGraphBranchEndpoints MediaGraphBuilder::addVideoTranscodeBranch(const MediaGraphVideoBranchOptions& options)
{
    MediaGraphBranchEndpoints ep;

    // Decode
    auto decode = addNode(MediaNodeKind::VideoDecode, options.prefix + "_decode");
    addInput(decode, "packet_in", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    addOutput(decode, "frame_out", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);

    // Frame rate
    auto fr = addNode(MediaNodeKind::VideoFrameRate, options.prefix + "_fps");
    addInput(fr, "frame_in", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    addOutput(fr, "frame_out", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);

    // Filter
    auto filter = addNode(MediaNodeKind::VideoFilter, options.prefix + "_filter");
    addInput(filter, "frame_in", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    addOutput(filter, "frame_out", MediaStreamKind::Video, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);

    MediaNodeId last = decode;
    std::string lastPort = "frame_out";

    connect(decode, "frame_out", fr, "frame_in");
    connect(fr, "frame_out", filter, "frame_in");

    if (options.useHardwareTransfer) {
        auto hw = addNode(MediaNodeKind::HardwareTransfer, options.prefix + "_hw");
        addInput(hw, "frame_in", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
        addOutput(hw, "frame_out", MediaStreamKind::Video, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);

        connect(filter, "frame_out", hw, "frame_in");

        ep.inputNode = decode;
        ep.inputPort = "packet_in";
        ep.outputNode = hw;
        ep.outputPort = "frame_out";
        return ep;
    }

    ep.inputNode = decode;
    ep.inputPort = "packet_in";
    ep.outputNode = filter;
    ep.outputPort = "frame_out";

    return ep;
}

} // namespace media::ffmpeg::graph

#include "internal/graph/builder/realtime/MediaRealtimeVideoEncodingGroupBuilder.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {

::media::Result<MediaEncodedBranchEndpoints>
MediaRealtimeVideoEncodingGroupBuilder::appendFanout(
    MediaGraph& graph,
    const std::string& prefix,
    MediaEncodedBranchEndpoints encoded,
    const MediaVideoOutputFanoutPlan& policy,
    const MediaEdgePolicy& packetPolicy)
{
    using Result = ::media::Result<MediaEncodedBranchEndpoints>;
    constexpr auto owner = "MediaRealtimeVideoEncodingGroupBuilder";
    auto* codec = graph.findOutputPort(encoded.codec.node, encoded.codec.port);
    const auto* packet = graph.findOutputPort(encoded.packet.node, encoded.packet.port);
    const auto* encoder = graph.findNode(encoded.packet.node);
    if (prefix.empty() || !codec || !packet || !encoder ||
        encoder->kind != MediaNodeKind::VideoEncode ||
        policy.noOutputs != MediaVideoNoOutputPolicy::Consume ||
        policy.overflow != MediaVideoOutputOverflowPolicy::FailBranch) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "encoding group requires its encoder endpoints and typed distributor policy"));
    }
    codec->required = false;
    codec->multiple = true;
    if (!graph.findOutputPort(encoded.codec.node, "codec_parameters")) return Result::failure(
        ::media::ErrorInfo::notInitialized("Encoding group has no immutable codec parameters output"));
    encoded.codec.port = "codec_parameters";
    const auto format = packet->format;
    const auto fanout = graph.addNode(MediaNodeKind::EncodedVideoOutputFanout,
        prefix + ".encoded_output_fanout", "Encoded video output distributor");
    for (const auto& option : {std::pair{"fanout.zero_outputs", "consume"},
                              std::pair{"fanout.overflow", "fail_branch"}}) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, fanout, option.first, option.second); !status) {
            return Result::failure(status.error());
        }
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner,
        fanout, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet, true, false); !status) return Result::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
        graph, owner, fanout, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, false, true,
        format); !status) return Result::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner,
        encoded.packet.node, encoded.packet.port, fanout, "packet",
        prefix + ".encoded_output_fanout.packet", packetPolicy); !status) {
        return Result::failure(status.error());
    }
    encoded.packet = {fanout, "packet"};
    return Result::success(std::move(encoded));
}

} // namespace media::ffmpeg::graph

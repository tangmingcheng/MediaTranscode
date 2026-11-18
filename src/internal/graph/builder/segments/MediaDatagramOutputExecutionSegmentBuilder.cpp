#include "internal/graph/builder/segments/MediaDatagramOutputExecutionSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"

#include <string>
#include <tuple>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner =
    "MediaDatagramOutputExecutionSegmentBuilder";

} // namespace

::media::Status MediaDatagramOutputExecutionSegmentBuilder::validateSessionAvailable(
    const MediaGraph& graph,
    const MediaProtocolOutputSessionKey& sessionKey)
{
    if (!sessionKey.valid()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("Datagram output requires a planned session authority"));
    // Every complete RTP or MPEG-TS output owns exactly one common transport
    // plan source. Its typed session, not protocol kind or node prefix, defines
    // the authority domain even when different protocols share one DAG.
    for (const auto& node : graph.nodes()) {
        if (node.kind != MediaNodeKind::DatagramTransportPlanSource) continue;
        auto existing = MediaDatagramTransportPlanSourceNodePlanCodec::decode(node);
        if (!existing) return ::media::Status::failure(existing.error());
        if (existing.value().sessionKey() == sessionKey.value()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Datagram output rejects duplicate authority in its output session"));
        }
    }
    return ::media::Status::success();
}

::media::Result<MediaDatagramOutputExecutionSegmentResult>
MediaDatagramOutputExecutionSegmentBuilder::build(
    MediaGraph& graph,
    const MediaDatagramOutputExecutionSegmentOptions& options)
{
    using Result = ::media::Result<MediaDatagramOutputExecutionSegmentResult>;
    const MediaPort* activation = options.activation.valid()
        ? graph.findOutputPort(options.activation.node, options.activation.port)
        : nullptr;
    auto streamSet = MediaTranscodeStreamSetCodec::encode(options.streamSet);
    if (options.prefix.empty() || !activation || !options.sessionKey.valid() ||
        !options.transportPlan || !options.edgePolicies || !streamSet ||
        options.transportPlan->sessionKey() != options.sessionKey.value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram execution segment requires activation and one complete transport product"));
    }
    if (auto available = validateSessionAvailable(graph, options.sessionKey); !available) {
        return Result::failure(available.error());
    }
    auto declaration=MediaOutputSegmentPlanner::execution(*options.edgePolicies, *std::pmr::new_delete_resource());
    if(!declaration)return Result::failure(declaration.error());
    const MediaNodeId source = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::TransportPlan),
        options.prefix + ".transport.plan", "Activated datagram transport plan");
    const MediaNodeId sender = graph.addNode(
        *declaration.value().kind(MediaOutputSegmentRole::Sender),
        options.prefix + ".transport.sender",
        "Common GCRA-paced nonblocking datagram sender");
    if (!source.isValid() || !sender.isValid()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "Datagram execution segment failed to add its nodes"));
    }
    auto encoded = MediaDatagramTransportPlanSourceNodePlanCodec::apply(
        graph, source, *options.transportPlan);
    if (!encoded) return Result::failure(encoded.error());
    for (const auto& [key, value] : {
             std::pair{"scheduled_datagram_sender.session",
                       options.sessionKey.value()},
             std::pair{"scheduled_datagram_sender.stream_set",
                       std::string(streamSet.value())}}) {
        auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, sender, key, value);
        if (!set) return Result::failure(set.error());
    }
    using MediaGraphBuildSupport::addInputPortChecked;
    using MediaGraphBuildSupport::addOutputPortChecked;
    for (const auto& port : {
             std::tuple{source, "activation", MediaEdgeKind::Event,
                        MediaPayloadKind::GraphEvent, false},
             std::tuple{sender, "plan", MediaEdgeKind::Metadata,
                        MediaPayloadKind::DatagramTransportPlan, false},
             std::tuple{sender, "batch", MediaEdgeKind::ScheduledDatagramBatch,
                        MediaPayloadKind::WireDatagramBatch, true}}) {
        auto added = addInputPortChecked(
            graph, Owner, std::get<0>(port), std::get<1>(port),
            MediaStreamKind::Metadata, std::get<2>(port), std::get<3>(port),
            true, std::get<4>(port));
        if (!added) return Result::failure(added.error());
    }
    auto output = addOutputPortChecked(
        graph, Owner, source, "plan", MediaStreamKind::Metadata,
        MediaEdgeKind::Metadata, MediaPayloadKind::DatagramTransportPlan,
        true, true);
    if (!output) return Result::failure(output.error());
    MediaOutputSegmentBindings bindings{std::move(declaration).value(),{}};
    bindings.endpoint(MediaOutputSegmentRole::Activation)=options.activation;
    bindings.endpoint(MediaOutputSegmentRole::TransportPlan)={source,""};
    bindings.endpoint(MediaOutputSegmentRole::Sender)={sender,""};
    if(auto status=bindings.connect(graph,Owner);!status)return Result::failure(status.error());
    return Result::success({source,sender,std::move(bindings)});
}

} // namespace media::ffmpeg::graph

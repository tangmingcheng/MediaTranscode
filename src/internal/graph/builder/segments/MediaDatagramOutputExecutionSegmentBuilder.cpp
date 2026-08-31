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
    const MediaNodeId source = graph.addNode(
        MediaNodeKind::DatagramTransportPlanSource,
        options.prefix + ".transport.plan", "Activated datagram transport plan");
    const MediaNodeId sender = graph.addNode(
        MediaNodeKind::ScheduledDatagramSender,
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
    auto connected = MediaGraphBuildSupport::connectChecked(
        graph, Owner, options.activation.node, options.activation.port,
        source, "activation", "activation -> datagram transport plan",
        options.edgePolicies->atomicMetadata);
    if (!connected) return Result::failure(connected.error());
    connected = MediaGraphBuildSupport::connectChecked(
        graph, Owner, source, "plan", sender, "plan",
        "transport plan -> common sender", options.edgePolicies->atomicMetadata);
    if (!connected) return Result::failure(connected.error());
    return Result::success({source, sender});
}

::media::Status MediaDatagramOutputExecutionSegmentBuilder::connectWireSource(
    MediaGraph& graph,
    const MediaDatagramOutputExecutionSegmentResult& execution,
    MediaEndpoint wireSource,
    const MediaEdgePolicy& policy,
    const char* label)
{
    return MediaGraphBuildSupport::connectChecked(
        graph, Owner, wireSource.node, wireSource.port,
        execution.sender, "batch", label, policy);
}

::media::Status
MediaDatagramOutputExecutionSegmentBuilder::connectTransportConsumer(
    MediaGraph& graph,
    const MediaDatagramOutputExecutionSegmentResult& execution,
    MediaNodeId consumer,
    const char* port,
    const MediaEdgePolicy& policy,
    const char* label)
{
    return MediaGraphBuildSupport::connectChecked(
        graph, Owner, execution.transportPlanSource, "plan",
        consumer, port, label, policy);
}

} // namespace media::ffmpeg::graph

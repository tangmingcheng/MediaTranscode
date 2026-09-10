#include "application/realtime/MediaRealtimeSegmentFactory.h"

#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/nodes/video/EncodedVideoOutputFanoutNode.h"

#include <algorithm>
#include <set>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeCreatedSegment> MediaRealtimeSegmentFactory::create(
    std::uint64_t segmentId, std::shared_ptr<const MediaGraph> graph,
    const std::vector<MediaNodeId>& nodes, MediaThreadingPolicy threading,
    std::shared_ptr<MediaRuntimeBranchResourceReservation> resources,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
    std::shared_ptr<MediaDatagramServiceScopeArbiter> serviceScope,
    MediaBufferRef preparedEncoder, MediaGraphExecutionContext& session,
    const ExportSource& exportSource, const MediaRuntimeReclamationPlan& reclamationPlan)
{
    using Result = ::media::Result<MediaRealtimeCreatedSegment>;
    CodecResolverNode* resolver = nullptr;
    EncodedVideoOutputFanoutNode* fanout = nullptr;
    std::vector<std::unique_ptr<MediaRuntimeNode>> runtimeNodes;
    for (const auto id : nodes) {
        const auto* node = graph->findNode(id);
        if (!node) return Result::failure(::media::ErrorInfo::invalidArgument("segment node is absent from its graph"));
        auto created = node->kind == MediaNodeKind::VideoOutputScheduler
            ? MediaRuntimeNodeFactory::createVideoOutputScheduler(*node, authority)
            : node->kind == MediaNodeKind::MpegTsRtpSdpPublisher
            ? MediaRuntimeNodeFactory::createMpegTsRtpSdpPublisher(*node, authority)
            : MediaRuntimeNodeFactory::create(*node, nullptr, nullptr, authority, serviceScope);
        if (!created) return Result::failure(created.error());
        if (auto* candidate = dynamic_cast<CodecResolverNode*>(created.value().get())) {
            resolver = candidate;
            if (preparedEncoder) {
                auto bound = resolver->bindPreparedEncoder(preparedEncoder);
                if (!bound) return Result::failure(bound.error());
            }
        }
        if (auto* candidate = dynamic_cast<EncodedVideoOutputFanoutNode*>(created.value().get())) fanout = candidate;
        runtimeNodes.push_back(std::move(created).value());
    }
    std::vector<MediaRuntimeSegmentOutputBinding> upstream;
    std::set<std::uint32_t> exported;
    const auto belongs = [&](MediaNodeId id) { return std::find(nodes.begin(), nodes.end(), id) != nodes.end(); };
    for (const auto& edge : graph->edges()) {
        if (!belongs(edge.to.nodeId) || belongs(edge.from.nodeId) || !exported.insert(edge.from.portId.value).second) continue;
        auto binding = exportSource(edge.from.nodeId, edge.from.portId);
        if (!binding) return Result::failure(binding.error());
        upstream.push_back(std::move(binding).value());
    }
    auto branch = MediaRuntimeBranch::prepare({segmentId, threading, reclamationPlan, std::move(graph), nodes,
        std::move(upstream), std::move(runtimeNodes), std::move(resources)}, session);
    if (!branch) return Result::failure(branch.error());
    return Result::success({std::move(branch).value(), resolver, fanout});
}

} // namespace media::ffmpeg::graph

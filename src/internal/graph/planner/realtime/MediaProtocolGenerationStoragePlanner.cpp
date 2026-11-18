#include "internal/graph/planner/realtime/MediaProtocolGenerationStoragePlanner.h"
#include "internal/graph/nodes/output/MediaMpegTsDatagramMaterializerNode.h"
#include "internal/graph/nodes/output/MediaRtpDatagramMaterializerNode.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>

namespace media::ffmpeg::graph {
::media::Result<MediaProtocolGenerationStoragePlan> MediaProtocolGenerationStoragePlanner::plan(
    const MediaGraph& graph, std::span<const MediaNodeId> selectedNodes)
{
    using Result = ::media::Result<MediaProtocolGenerationStoragePlan>;
    MediaProtocolGenerationStoragePlan result;
    try {
        for (const auto& node : graph.nodes()) {
            if (!selectedNodes.empty() && std::find(selectedNodes.begin(), selectedNodes.end(), node.id) == selectedNodes.end())
                continue;
            if (node.kind != MediaNodeKind::MpegTsDatagramMaterializer &&
                node.kind != MediaNodeKind::RtpDatagramMaterializer) continue;
            const auto* sealed = graph.protocolGenerationStorageFor(node.id);
            auto geometry = sealed
                ? ::media::Result<MediaProtocolGenerationStorageGeometry>::success(sealed->geometry)
                : node.kind == MediaNodeKind::MpegTsDatagramMaterializer
                ? MediaMpegTsMaterializerGenerationSession::storageGeometry()
                : MediaRtpMaterializerGenerationSession::storageGeometry();
            if (!geometry) return Result::failure(geometry.error());
            auto total = MediaCheckedArithmetic::add(result.admittedBytes, geometry.value().totalBytes,
                "Protocol generation fixed storage");
            if (!total) return Result::failure(total.error());
            result.instances.push_back({node.id, node.kind, geometry.value()});
            result.admittedBytes = total.value();
        }
        return Result::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Protocol generation storage plan"));
    }
}
} // namespace media::ffmpeg::graph

#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeOptionApplier";

} // namespace

::media::Result<void> MediaRealtimeOptionApplier::applyInputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.inputUrl.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "url", options.inputUrl); !status) return status;
    }
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applyOutputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.outputUrl.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "url", options.outputUrl); !status) return status;
    }
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applySdpWriterOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (!options.sdpPath.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "path", options.sdpPath); !status) return status;
    }
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph

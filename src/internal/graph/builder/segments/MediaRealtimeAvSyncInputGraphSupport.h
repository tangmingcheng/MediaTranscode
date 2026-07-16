#pragma once

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputEndpoints.h"
#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaRealtimeAvSyncInputGraphSupport final {
public:
    static ::media::Result<void> setOption(
        MediaGraph& graph,
        MediaNodeId node,
        std::string_view key,
        std::string value);

    static ::media::Result<void> addInput(
        MediaGraph& graph,
        MediaNodeId node,
        std::string name,
        MediaStreamKind stream,
        MediaEdgeKind edge,
        MediaPayloadKind payload);

    static ::media::Result<void> addOutput(
        MediaGraph& graph,
        MediaNodeId node,
        std::string name,
        MediaStreamKind stream,
        MediaEdgeKind edge,
        MediaPayloadKind payload,
        bool required = true,
        bool multiple = false);

    static ::media::Result<void> validateOutput(
        const MediaGraph& graph,
        const MediaEndpoint& endpoint,
        MediaStreamKind stream,
        MediaEdgeKind edge,
        MediaPayloadKind payload);

    static ::media::Result<void> connect(
        MediaGraph& graph,
        const MediaEndpoint& from,
        MediaNodeId to,
        std::string toPort,
        std::string label,
        const MediaEdgePolicy& policy);

    static ::media::Result<void> connect(
        MediaGraph& graph,
        MediaNodeId from,
        std::string fromPort,
        MediaNodeId to,
        std::string toPort,
        std::string label,
        const MediaEdgePolicy& policy);

    static ::media::Result<MediaNodeId> addNode(
        MediaGraph& graph,
        MediaNodeKind kind,
        std::string name,
        std::string diagnostic);

private:
    MediaRealtimeAvSyncInputGraphSupport() = delete;
};

} // namespace media::ffmpeg::graph

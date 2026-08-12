#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/core/MediaNodeOptions.h"

#include <initializer_list>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaGraphShapeQuery final {
public:
    static bool hasExactOptionKeys(
        const MediaNodeOptions& options,
        std::initializer_list<std::string_view> expected);
    static bool matchesStreamSetOption(
        const MediaNodeOptions& options,
        std::string_view key,
        MediaTranscodeStreamSet expected);
    static bool validPort(
        const MediaPort* port,
        MediaPortDirection direction,
        MediaStreamKind stream,
        MediaEdgeKind edge,
        MediaPayloadKind payload) noexcept;
    static const MediaEdge* singleEdge(
        const MediaGraph& graph,
        MediaPortId from,
        MediaPortId to) noexcept;
    static const MediaEdge* singleIncomingEdge(
        const MediaGraph& graph,
        MediaPortId target) noexcept;
    static std::size_t incomingEdgeCount(
        const MediaGraph& graph,
        MediaPortId target) noexcept;
};

} // namespace media::ffmpeg::graph

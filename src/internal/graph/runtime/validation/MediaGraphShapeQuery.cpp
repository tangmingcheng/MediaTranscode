#include "internal/graph/runtime/validation/MediaGraphShapeQuery.h"

#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"

#include <string>

namespace media::ffmpeg::graph {

bool MediaGraphShapeQuery::hasExactOptionKeys(
    const MediaNodeOptions& options,
    std::initializer_list<std::string_view> expected)
{
    if (options.values().size() != expected.size()) return false;
    for (std::string_view key : expected) {
        if (!options.has(std::string(key))) return false;
    }
    return true;
}

bool MediaGraphShapeQuery::matchesStreamSetOption(
    const MediaNodeOptions& options,
    std::string_view key,
    MediaTranscodeStreamSet expected)
{
    auto encoded = MediaTranscodeStreamSetCodec::encode(expected);
    return encoded && options.has(std::string(key)) &&
        options.value(std::string(key)) == encoded.value();
}

bool MediaGraphShapeQuery::validPort(
    const MediaPort* port,
    MediaPortDirection direction,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload) noexcept
{
    return port && port->direction == direction &&
        port->streamKind == stream && port->edgeKind == edge &&
        port->payloadKind == payload;
}

const MediaEdge* MediaGraphShapeQuery::singleEdge(
    const MediaGraph& graph,
    MediaPortId from,
    MediaPortId to) noexcept
{
    const MediaEdge* match = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.from.portId != from || edge.to.portId != to) continue;
        if (match) return nullptr;
        match = &edge;
    }
    return match;
}

const MediaEdge* MediaGraphShapeQuery::singleIncomingEdge(
    const MediaGraph& graph,
    MediaPortId target) noexcept
{
    const MediaEdge* match = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId != target) continue;
        if (match) return nullptr;
        match = &edge;
    }
    return match;
}

std::size_t MediaGraphShapeQuery::incomingEdgeCount(
    const MediaGraph& graph,
    MediaPortId target) noexcept
{
    std::size_t count = 0;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId == target) ++count;
    }
    return count;
}

} // namespace media::ffmpeg::graph

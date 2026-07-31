#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"

#include <string>

namespace media::ffmpeg::graph {

MediaAvSyncGraphShape::MediaAvSyncGraphShape(
    const MediaGraph& graph) noexcept
    : m_graph(graph)
{
}

std::size_t MediaAvSyncGraphShape::count(
    MediaNodeKind kind) const noexcept
{
    std::size_t result = 0;
    for (const MediaNode& node : m_graph.nodes()) {
        if (node.kind == kind) ++result;
    }
    return result;
}

std::vector<const MediaNode*> MediaAvSyncGraphShape::nodes(
    MediaNodeKind kind) const
{
    std::vector<const MediaNode*> result;
    for (const MediaNode& node : m_graph.nodes()) {
        if (node.kind == kind) result.push_back(&node);
    }
    return result;
}

::media::Status MediaAvSyncGraphShape::requireExact(
    std::initializer_list<MediaNodeCardinality> expected,
    std::string_view owner) const
{
    for (const auto& requirement : expected) {
        const std::size_t actual = count(requirement.kind);
        if (actual != requirement.count) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    std::string(owner) + " requires exactly " +
                    std::to_string(requirement.count) + " " +
                    std::string(requirement.label) + " node(s), found " +
                    std::to_string(actual)));
        }
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph

#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <span>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaNodeCardinality final {
    MediaNodeKind kind;
    std::size_t count;
    std::string_view label;
};

class MediaAvSyncGraphShape final {
public:
    explicit MediaAvSyncGraphShape(const MediaGraph& graph) noexcept;

    MediaAvSyncGraphShape(const MediaGraph& graph, std::span<const MediaNodeId> members) noexcept;

    std::size_t count(MediaNodeKind kind) const noexcept;
    std::vector<const MediaNode*> nodes(MediaNodeKind kind) const;
    ::media::Status requireExact(
        std::initializer_list<MediaNodeCardinality> expected,
        std::string_view owner) const;

private:
    const MediaGraph& m_graph;
    std::optional<std::span<const MediaNodeId>> m_members;
    bool contains(MediaNodeId id) const noexcept;
};

} // namespace media::ffmpeg::graph

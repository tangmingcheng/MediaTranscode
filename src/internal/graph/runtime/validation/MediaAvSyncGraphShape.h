#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <initializer_list>
#include <string_view>
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

    std::size_t count(MediaNodeKind kind) const noexcept;
    std::vector<const MediaNode*> nodes(MediaNodeKind kind) const;
    ::media::Status requireExact(
        std::initializer_list<MediaNodeCardinality> expected,
        std::string_view owner) const;

private:
    const MediaGraph& m_graph;
};

} // namespace media::ffmpeg::graph

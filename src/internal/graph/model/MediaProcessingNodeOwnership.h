#pragma once

#include "internal/graph/core/MediaNodeId.h"

#include <algorithm>
#include <vector>

namespace media::ffmpeg::graph {

// Processing ownership is distinct from the currently graph-wide transition scope.
struct MediaProcessingNodeOwnership final {
    std::vector<MediaNodeId> source;
    std::vector<MediaNodeId> output;

    void append(const MediaProcessingNodeOwnership& other)
    {
        source.insert(source.end(), other.source.begin(), other.source.end());
        output.insert(output.end(), other.output.begin(), other.output.end());
    }

    bool contains(MediaNodeId id) const
    {
        return std::find(source.begin(), source.end(), id) != source.end() ||
            std::find(output.begin(), output.end(), id) != output.end();
    }

    template<class Visitor>
    void forEach(Visitor&& visitor) const
    {
        for (const auto id : source) visitor(id);
        for (const auto id : output) visitor(id);
    }
};

} // namespace media::ffmpeg::graph

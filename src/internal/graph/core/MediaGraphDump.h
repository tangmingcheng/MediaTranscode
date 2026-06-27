#pragma once

#include "internal/graph/core/MediaGraph.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaGraphDump {
public:
    static std::string toText(const MediaGraph& graph);

    static std::string toGraphvizDot(const MediaGraph& graph,
                                     const std::string& graphName = "MediaGraph");
};

} // namespace media::ffmpeg::graph

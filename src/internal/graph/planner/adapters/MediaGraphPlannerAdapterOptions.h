#pragma once

#include <string>

namespace media::ffmpeg::graph {

struct MediaGraphPlannerAdapterOptions {
    std::string host = "127.0.0.1";
    std::string zone = "local";

    std::string localNodeId = "local";
    std::string edgeNodeId = "edge";
    std::string workerNodeId = "worker";

    int basePort = 19000;

    bool enableGpuPlanning = false;
    bool enableMeshPlanning = false;
    bool preferZeroCopy = true;
};

} // namespace media::ffmpeg::graph

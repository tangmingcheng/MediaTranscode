#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace media::ffmpeg::graph;

int fail(const std::string& message)
{
    std::cerr << "graph planner probe failed: " << message << '\n';
    return 1;
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    return fail(action + ": " + status.error().describe());
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: media_transcode_graph_planner_probe.exe <input-media-file> [output-media-file] [output-codec] [--log|--quiet]\n";
        return 2;
    }

    MediaPipelinePlannerOptions options;
    const std::string inputPath = argv[1];
    options.diagnosticLogEnabled = true;

    std::vector<std::string> positional;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--quiet" || arg == "--no-log") {
            options.diagnosticLogEnabled = false;
            continue;
        }
        if (arg == "--log") {
            options.diagnosticLogEnabled = true;
            continue;
        }
        positional.push_back(arg);
    }

    options.outputPath = positional.size() >= 1 ? positional[0] : "planned-output.mp4";
    options.outputCodecName = positional.size() >= 2 ? positional[1] : "h264";

    auto buildResult = MediaPipelinePlanner::buildPlannedVideoFileTranscodeGraph(inputPath, options);
    if (!buildResult) {
        return fail("build planned graph: " + buildResult.error().describe());
    }

    MediaGraphRuntime runtime;
    auto compileStatus = runtime.compile(std::move(buildResult.value().graph));
    if (!compileStatus) {
        return failStatus("compile planned graph", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    std::cout << "graph planner probe ok: "
              << "graph_nodes=" << runtime.graph()->nodeCount()
              << ", graph_edges=" << runtime.graph()->edgeCount()
              << '\n';

    return 0;
}
